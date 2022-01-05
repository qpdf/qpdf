#include <qpdf/QPDFJob.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <fcntl.h>
#include <iostream>
#include <memory>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/PointerHolder.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFArgParser.hh>

#include <qpdf/QPDFWriter.hh>
#include <qpdf/QIntC.hh>

namespace
{
    class ImageOptimizer: public QPDFObjectHandle::StreamDataProvider
    {
      public:
        ImageOptimizer(QPDFJob& o, QPDFObjectHandle& image);
        virtual ~ImageOptimizer()
        {
        }
        virtual void provideStreamData(int objid, int generation,
                                       Pipeline* pipeline);
        PointerHolder<Pipeline> makePipeline(
            std::string const& description, Pipeline* next);
        bool evaluate(std::string const& description);

      private:
        QPDFJob& o;
        QPDFObjectHandle image;
    };

    class DiscardContents: public QPDFObjectHandle::ParserCallbacks
    {
      public:
        virtual ~DiscardContents() {}
        virtual void handleObject(QPDFObjectHandle) {}
        virtual void handleEOF() {}
    };

    struct QPDFPageData
    {
        QPDFPageData(std::string const& filename,
                     QPDF* qpdf, char const* range);
        QPDFPageData(QPDFPageData const& other, int page);

        std::string filename;
        QPDF* qpdf;
        std::vector<QPDFObjectHandle> orig_pages;
        std::vector<int> selected_pages;
    };

    class ProgressReporter: public QPDFWriter::ProgressReporter
    {
      public:
        ProgressReporter(std::ostream& cout,
                         std::string const& prefix,
                         char const* filename) :
            cout(cout),
            prefix(prefix),
            filename(filename)
        {
        }
        virtual ~ProgressReporter()
        {
        }

        virtual void reportProgress(int);
      private:
        std::ostream& cout;
        std::string prefix;
        std::string filename;
    };
}

ImageOptimizer::ImageOptimizer(QPDFJob& o, QPDFObjectHandle& image) :
    o(o),
    image(image)
{
}

PointerHolder<Pipeline>
ImageOptimizer::makePipeline(std::string const& description, Pipeline* next)
{
    PointerHolder<Pipeline> result;
    QPDFObjectHandle dict = image.getDict();
    QPDFObjectHandle w_obj = dict.getKey("/Width");
    QPDFObjectHandle h_obj = dict.getKey("/Height");
    QPDFObjectHandle colorspace_obj = dict.getKey("/ColorSpace");
    if (! (w_obj.isNumber() && h_obj.isNumber()))
    {
        if (! description.empty())
        {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because image dictionary"
                     << " is missing required keys" << std::endl;
            });
        }
        return result;
    }
    QPDFObjectHandle components_obj = dict.getKey("/BitsPerComponent");
    if (! (components_obj.isInteger() && (components_obj.getIntValue() == 8)))
    {
        QTC::TC("qpdf", "qpdf image optimize bits per component");
        if (! description.empty())
        {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because image has other than"
                     << " 8 bits per component" << std::endl;
            });
        }
        return result;
    }
    // Files have been seen in the wild whose width and height are
    // floating point, which is goofy, but we can deal with it.
    JDIMENSION w = 0;
    if (w_obj.isInteger())
    {
        w = w_obj.getUIntValueAsUInt();
    }
    else
    {
        w = static_cast<JDIMENSION>(w_obj.getNumericValue());
    }
    JDIMENSION h = 0;
    if (h_obj.isInteger())
    {
        h = h_obj.getUIntValueAsUInt();
    }
    else
    {
        h = static_cast<JDIMENSION>(h_obj.getNumericValue());
    }
    std::string colorspace = (colorspace_obj.isName() ?
                              colorspace_obj.getName() :
                              std::string());
    int components = 0;
    J_COLOR_SPACE cs = JCS_UNKNOWN;
    if (colorspace == "/DeviceRGB")
    {
        components = 3;
        cs = JCS_RGB;
    }
    else if (colorspace == "/DeviceGray")
    {
        components = 1;
        cs = JCS_GRAYSCALE;
    }
    else if (colorspace == "/DeviceCMYK")
    {
        components = 4;
        cs = JCS_CMYK;
    }
    else
    {
        QTC::TC("qpdf", "qpdf image optimize colorspace");
        if (! description.empty())
        {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because qpdf can't optimize"
                     << " images with this colorspace" << std::endl;
            });
        }
        return result;
    }
    if (((o.oi_min_width > 0) && (w <= o.oi_min_width)) ||
        ((o.oi_min_height > 0) && (h <= o.oi_min_height)) ||
        ((o.oi_min_area > 0) && ((w * h) <= o.oi_min_area)))
    {
        QTC::TC("qpdf", "qpdf image optimize too small");
        if (! description.empty())
        {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because image"
                     << " is smaller than requested minimum dimensions"
                     << std::endl;
            });
        }
        return result;
    }

    result = new Pl_DCT("jpg", next, w, h, components, cs);
    return result;
}

bool
ImageOptimizer::evaluate(std::string const& description)
{
    if (! image.pipeStreamData(0, 0, qpdf_dl_specialized, true))
    {
        QTC::TC("qpdf", "qpdf image optimize no pipeline");
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": " << description
                 << ": not optimizing because unable to decode data"
                 << " or data already uses DCT"
                 << std::endl;
        });
        return false;
    }
    Pl_Discard d;
    Pl_Count c("count", &d);
    PointerHolder<Pipeline> p = makePipeline(description, &c);
    if (p.getPointer() == 0)
    {
        // message issued by makePipeline
        return false;
    }
    if (! image.pipeStreamData(p.getPointer(), 0, qpdf_dl_specialized))
    {
        return false;
    }
    long long orig_length = image.getDict().getKey("/Length").getIntValue();
    if (c.getCount() >= orig_length)
    {
        QTC::TC("qpdf", "qpdf image optimize no shrink");
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": " << description
                 << ": not optimizing because DCT compression does not"
                 << " reduce image size" << std::endl;
        });
        return false;
    }
    o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": " << description
             << ": optimizing image reduces size from "
             << orig_length << " to " << c.getCount()
             << std::endl;
    });
    return true;
}

void
ImageOptimizer::provideStreamData(int, int, Pipeline* pipeline)
{
    PointerHolder<Pipeline> p = makePipeline("", pipeline);
    if (p.getPointer() == 0)
    {
        // Should not be possible
        image.warnIfPossible("unable to create pipeline after previous"
                             " success; image data will be lost");
        pipeline->finish();
        return;
    }
    image.pipeStreamData(p.getPointer(), 0, qpdf_dl_specialized,
                         false, false);
}

QPDFPageData::QPDFPageData(std::string const& filename,
                           QPDF* qpdf,
                           char const* range) :
    filename(filename),
    qpdf(qpdf),
    orig_pages(qpdf->getAllPages())
{
    try
    {
        this->selected_pages =
            QUtil::parse_numrange(range,
                                  QIntC::to_int(this->orig_pages.size()));
    }
    catch (std::runtime_error& e)
    {
        throw std::runtime_error(
            "parsing numeric range for " + filename + ": " + e.what());
    }
}

QPDFPageData::QPDFPageData(QPDFPageData const& other, int page) :
    filename(other.filename),
    qpdf(other.qpdf),
    orig_pages(other.orig_pages)
{
    this->selected_pages.push_back(page);
}

void
ProgressReporter::reportProgress(int percentage)
{
    this->cout << prefix << ": " << filename << ": write progress: "
               << percentage << "%" << std::endl;
}

QPDFJob::Members::Members() :
    message_prefix("qpdf"),
    warnings(false),
    creates_output(false),
    cout(&std::cout),
    cerr(&std::cerr),
    encryption_status(0)
{
}

QPDFJob::QPDFJob() :
    password(0),
    linearize(false),
    decrypt(false),
    split_pages(0),
    verbose(false),
    progress(false),
    suppress_warnings(false),
    copy_encryption(false),
    encryption_file(0),
    encryption_file_password(0),
    encrypt(false),
    password_is_hex_key(false),
    suppress_password_recovery(false),
    password_mode(pm_auto),
    allow_insecure(false),
    allow_weak_crypto(false),
    keylen(0),
    r2_print(true),
    r2_modify(true),
    r2_extract(true),
    r2_annotate(true),
    r3_accessibility(true),
    r3_extract(true),
    r3_assemble(true),
    r3_annotate_and_form(true),
    r3_form_filling(true),
    r3_modify_other(true),
    r3_print(qpdf_r3p_full),
    force_V4(false),
    force_R5(false),
    cleartext_metadata(false),
    use_aes(false),
    stream_data_set(false),
    stream_data_mode(qpdf_s_compress),
    compress_streams(true),
    compress_streams_set(false),
    recompress_flate(false),
    recompress_flate_set(false),
    compression_level(-1),
    decode_level(qpdf_dl_generalized),
    decode_level_set(false),
    normalize_set(false),
    normalize(false),
    suppress_recovery(false),
    object_stream_set(false),
    object_stream_mode(qpdf_o_preserve),
    ignore_xref_streams(false),
    qdf_mode(false),
    preserve_unreferenced_objects(false),
    remove_unreferenced_page_resources(re_auto),
    keep_files_open(true),
    keep_files_open_set(false),
    keep_files_open_threshold(200), // default known in help and docs
    newline_before_endstream(false),
    coalesce_contents(false),
    flatten_annotations(false),
    flatten_annotations_required(0),
    flatten_annotations_forbidden(an_invisible | an_hidden),
    generate_appearances(false),
    show_npages(false),
    deterministic_id(false),
    static_id(false),
    static_aes_iv(false),
    suppress_original_object_id(false),
    show_encryption(false),
    show_encryption_key(false),
    check_linearization(false),
    show_linearization(false),
    show_xref(false),
    show_trailer(false),
    show_obj(0),
    show_gen(0),
    show_raw_stream_data(false),
    show_filtered_stream_data(false),
    show_pages(false),
    show_page_images(false),
    collate(0),
    flatten_rotation(false),
    list_attachments(false),
    json(false),
    check(false),
    optimize_images(false),
    externalize_inline_images(false),
    keep_inline_images(false),
    remove_page_labels(false),
    oi_min_width(128),      // Default values for these
    oi_min_height(128),     // oi flags are in --help
    oi_min_area(16384),     // and in the manual.
    ii_min_bytes(1024),     //
    underlay("underlay"),
    overlay("overlay"),
    under_overlay(0),
    require_outfile(true),
    replace_input(false),
    check_is_encrypted(false),
    check_requires_password(false),
    infilename(0),
    outfilename(0),
    m(new Members())
{
}


void
QPDFJob::setMessagePrefix(std::string const& message_prefix)
{
    this->m->message_prefix = message_prefix;
}

void
QPDFJob::setOutputStreams(std::ostream* out, std::ostream* err)
{
    this->m->cout = out ? out : &std::cout;
    this->m->cerr = err ? err : &std::cerr;
}

void
QPDFJob::doIfVerbose(
    std::function<void(std::ostream&, std::string const& prefix)> fn)
{
    if (this->verbose && (this->m->cout != nullptr))
    {
        fn(*(this->m->cout), this->m->message_prefix);
    }
}

void
QPDFJob::run()
{
    QPDFJob& o = *this; // QXXXQ
    PointerHolder<QPDF> pdf_ph;
    try
    {
        pdf_ph = processFile(o.infilename, o.password);
    }
    catch (QPDFExc& e)
    {
        if ((e.getErrorCode() == qpdf_e_password) &&
            (o.check_is_encrypted || o.check_requires_password))
        {
            // Allow --is-encrypted and --requires-password to
            // work when an incorrect password is supplied.
            this->m->encryption_status =
                qpdf_es_encrypted |
                qpdf_es_password_incorrect;
            return;
        }
        throw e;
    }
    QPDF& pdf = *pdf_ph;
    if (pdf.isEncrypted())
    {
        this->m->encryption_status = qpdf_es_encrypted;
    }

    if (o.check_is_encrypted || o.check_requires_password)
    {
        return;
    }
    bool other_warnings = false;
    std::vector<PointerHolder<QPDF>> page_heap;
    if (! o.page_specs.empty())
    {
        handlePageSpecs(pdf, other_warnings, page_heap);
    }
    if (! o.rotations.empty())
    {
        handleRotations(pdf);
    }
    handleUnderOverlay(pdf);
    handleTransformations(pdf);

    this->m->creates_output = ((o.outfilename != nullptr) || o.replace_input);
    if (! this->m->creates_output)
    {
        doInspection(pdf);
    }
    else if (o.split_pages)
    {
        doSplitPages(pdf, other_warnings);
    }
    else
    {
        writeOutfile(pdf);
    }
    if (! pdf.getWarnings().empty())
    {
        this->m->warnings = true;
    }
}

bool
QPDFJob::hasWarnings()
{
    return this->m->warnings;
}

bool
QPDFJob::createsOutput()
{
    return this->m->creates_output;
}

bool
QPDFJob::suppressWarnings()
{
    return this->suppress_warnings;
}

bool
QPDFJob::checkRequiresPassword()
{
    return this->check_requires_password;
}

bool
QPDFJob::checkIsEncrypted()
{
    return this->check_is_encrypted;
}

unsigned long
QPDFJob::getEncryptionStatus()
{
    return this->m->encryption_status;
}

void
QPDFJob::setQPDFOptions(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    if (o.ignore_xref_streams)
    {
        pdf.setIgnoreXRefStreams(true);
    }
    if (o.suppress_recovery)
    {
        pdf.setAttemptRecovery(false);
    }
    if (o.password_is_hex_key)
    {
        pdf.setPasswordIsHexKey(true);
    }
    if (o.suppress_warnings)
    {
        pdf.setSuppressWarnings(true);
    }
}

static std::string show_bool(bool v)
{
    return v ? "allowed" : "not allowed";
}

static std::string show_encryption_method(QPDF::encryption_method_e method)
{
    std::string result = "unknown";
    switch (method)
    {
      case QPDF::e_none:
        result = "none";
        break;
      case QPDF::e_unknown:
        result = "unknown";
        break;
      case QPDF::e_rc4:
        result = "RC4";
        break;
      case QPDF::e_aes:
        result = "AESv2";
        break;
      case QPDF::e_aesv3:
        result = "AESv3";
        break;
        // no default so gcc will warn for missing case
    }
    return result;
}

void
QPDFJob::showEncryption(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    // Extract /P from /Encrypt
    int R = 0;
    int P = 0;
    int V = 0;
    QPDF::encryption_method_e stream_method = QPDF::e_unknown;
    QPDF::encryption_method_e string_method = QPDF::e_unknown;
    QPDF::encryption_method_e file_method = QPDF::e_unknown;
    auto& cout = *this->m->cout;
    if (! pdf.isEncrypted(R, P, V,
                          stream_method, string_method, file_method))
    {
        cout << "File is not encrypted" << std::endl;
    }
    else
    {
        cout << "R = " << R << std::endl;
        cout << "P = " << P << std::endl;
        std::string user_password = pdf.getTrimmedUserPassword();
        std::string encryption_key = pdf.getEncryptionKey();
        cout << "User password = " << user_password << std::endl;
        if (o.show_encryption_key)
        {
            cout << "Encryption key = "
                 << QUtil::hex_encode(encryption_key) << std::endl;
        }
        if (pdf.ownerPasswordMatched())
        {
            cout << "Supplied password is owner password" << std::endl;
        }
        if (pdf.userPasswordMatched())
        {
            cout << "Supplied password is user password" << std::endl;
        }
        cout << "extract for accessibility: "
             << show_bool(pdf.allowAccessibility()) << std::endl
             << "extract for any purpose: "
             << show_bool(pdf.allowExtractAll()) << std::endl
             << "print low resolution: "
             << show_bool(pdf.allowPrintLowRes()) << std::endl
             << "print high resolution: "
             << show_bool(pdf.allowPrintHighRes()) << std::endl
             << "modify document assembly: "
             << show_bool(pdf.allowModifyAssembly()) << std::endl
             << "modify forms: "
             << show_bool(pdf.allowModifyForm()) << std::endl
             << "modify annotations: "
             << show_bool(pdf.allowModifyAnnotation()) << std::endl
             << "modify other: "
             << show_bool(pdf.allowModifyOther()) << std::endl
             << "modify anything: "
             << show_bool(pdf.allowModifyAll()) << std::endl;
        if (V >= 4)
        {
            cout << "stream encryption method: "
                 << show_encryption_method(stream_method) << std::endl
                 << "string encryption method: "
                 << show_encryption_method(string_method) << std::endl
                 << "file encryption method: "
                 << show_encryption_method(file_method) << std::endl;
        }
    }
}

void
QPDFJob::doCheck(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    // Code below may set okay to false but not to true.
    // We assume okay until we prove otherwise but may
    // continue to perform additional checks after finding
    // errors.
    bool okay = true;
    bool warnings = false;
    auto& cout = *this->m->cout;
    cout << "checking " << o.infilename << std::endl;
    try
    {
        int extension_level = pdf.getExtensionLevel();
        cout << "PDF Version: " << pdf.getPDFVersion();
        if (extension_level > 0)
        {
            cout << " extension level " << pdf.getExtensionLevel();
        }
        cout << std::endl;
        showEncryption(pdf);
        if (pdf.isLinearized())
        {
            cout << "File is linearized\n";
            // any errors or warnings are reported by
            // checkLinearization(). We treat all issues reported here
            // as warnings.
            if (! pdf.checkLinearization())
            {
                warnings = true;
            }
        }
        else
        {
            cout << "File is not linearized\n";
        }

        // Write the file no nowhere, uncompressing
        // streams.  This causes full file traversal and
        // decoding of all streams we can decode.
        QPDFWriter w(pdf);
        Pl_Discard discard;
        w.setOutputPipeline(&discard);
        w.setDecodeLevel(qpdf_dl_all);
        w.write();

        // Parse all content streams
        QPDFPageDocumentHelper dh(pdf);
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        DiscardContents discard_contents;
        int pageno = 0;
        for (std::vector<QPDFPageObjectHelper>::iterator iter =
                 pages.begin();
             iter != pages.end(); ++iter)
        {
            QPDFPageObjectHelper& page(*iter);
            ++pageno;
            try
            {
                page.parseContents(&discard_contents);
            }
            catch (QPDFExc& e)
            {
                okay = false;
                *(this->m->cerr)
                    << "ERROR: page " << pageno << ": "
                    << e.what() << std::endl;
            }
        }
    }
    catch (std::exception& e)
    {
        (*this->m->cerr) << "ERROR: " << e.what() << std::endl;
        okay = false;
    }
    if (! okay)
    {
        throw std::runtime_error("errors detected");
    }

    if ((! pdf.getWarnings().empty()) || warnings)
    {
        this->m->warnings = TRUE;
    }
    else
    {
        *(this->m->cout)
            << "No syntax or stream encoding errors"
            << " found; the file may still contain"
            << std::endl
            << "errors that qpdf cannot detect"
            << std::endl;
    }
}

void
QPDFJob::doShowObj(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    QPDFObjectHandle obj;
    if (o.show_trailer)
    {
        obj = pdf.getTrailer();
    }
    else
    {
        obj = pdf.getObjectByID(o.show_obj, o.show_gen);
    }
    bool error = false;
    if (obj.isStream())
    {
        if (o.show_raw_stream_data || o.show_filtered_stream_data)
        {
            bool filter = o.show_filtered_stream_data;
            if (filter &&
                (! obj.pipeStreamData(0, 0, qpdf_dl_all)))
            {
                QTC::TC("qpdf", "qpdf unable to filter");
                obj.warnIfPossible("unable to filter stream data");
                error = true;
            }
            else
            {
                QUtil::binary_stdout();
                Pl_StdioFile out("stdout", stdout);
                obj.pipeStreamData(
                    &out,
                    (filter && o.normalize) ? qpdf_ef_normalize : 0,
                    filter ? qpdf_dl_all : qpdf_dl_none);
            }
        }
        else
        {
            *(this->m->cout)
                << "Object is stream.  Dictionary:" << std::endl
                << obj.getDict().unparseResolved() << std::endl;
        }
    }
    else
    {
        *(this->m->cout) << obj.unparseResolved() << std::endl;
    }
    if (error)
    {
        throw std::runtime_error(
            "unable to get object " + obj.getObjGen().unparse());
    }
}

void
QPDFJob::doShowPages(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    int pageno = 0;
    auto& cout = *this->m->cout;
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFPageObjectHelper& ph(*iter);
        QPDFObjectHandle page = ph.getObjectHandle();
        ++pageno;

        cout << "page " << pageno << ": "
             << page.getObjectID() << " "
             << page.getGeneration() << " R" << std::endl;
        if (o.show_page_images)
        {
            std::map<std::string, QPDFObjectHandle> images = ph.getImages();
            if (! images.empty())
            {
                cout << "  images:" << std::endl;
                for (auto const& iter2: images)
                {
                    std::string const& name = iter2.first;
                    QPDFObjectHandle image = iter2.second;
                    QPDFObjectHandle dict = image.getDict();
                    int width =
                        dict.getKey("/Width").getIntValueAsInt();
                    int height =
                        dict.getKey("/Height").getIntValueAsInt();
                    cout << "    " << name << ": "
                         << image.unparse()
                         << ", " << width << " x " << height
                         << std::endl;
                }
            }
        }

        cout << "  content:" << std::endl;
        std::vector<QPDFObjectHandle> content =
            ph.getPageContents();
        for (auto& iter2: content)
        {
            cout << "    " << iter2.unparse() << std::endl;
        }
    }
}

void
QPDFJob::doListAttachments(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    if (efdh.hasEmbeddedFiles())
    {
        for (auto const& i: efdh.getEmbeddedFiles())
        {
            std::string const& key = i.first;
            auto efoh = i.second;
            *(this->m->cout)
                << key << " -> "
                << efoh->getEmbeddedFileStream().getObjGen()
                << std::endl;
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                auto desc = efoh->getDescription();
                if (! desc.empty())
                {
                    cout << "  description: " << desc << std::endl;
                }
                cout << "  preferred name: " << efoh->getFilename()
                          << std::endl;
                cout << "  all names:" << std::endl;
                for (auto const& i2: efoh->getFilenames())
                {
                    cout << "    " << i2.first << " -> " << i2.second
                              << std::endl;
                }
                cout << "  all data streams:" << std::endl;
                for (auto i2: efoh->getEmbeddedFileStreams().ditems())
                {
                    cout << "    " << i2.first << " -> "
                              << i2.second.getObjGen()
                              << std::endl;
                }
            });
        }
    }
    else
    {
        *(this->m->cout)
            << o.infilename << " has no embedded files" << std::endl;
    }
}

void
QPDFJob::doShowAttachment(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    auto fs = efdh.getEmbeddedFile(o.attachment_to_show);
    if (! fs)
    {
        throw std::runtime_error(
            "attachment " + o.attachment_to_show + " not found");
    }
    auto efs = fs->getEmbeddedFileStream();
    QUtil::binary_stdout();
    Pl_StdioFile out("stdout", stdout);
    efs.pipeStreamData(&out, 0, qpdf_dl_all);
}

void
QPDFJob::parse_object_id(std::string const& objspec,
                         bool& trailer, int& obj, int& gen)
{
    if (objspec == "trailer")
    {
        trailer = true;
    }
    else
    {
        trailer = false;
        obj = QUtil::string_to_int(objspec.c_str());
        size_t comma = objspec.find(',');
        if ((comma != std::string::npos) && (comma + 1 < objspec.length()))
        {
            gen = QUtil::string_to_int(
                objspec.substr(1 + comma, std::string::npos).c_str());
        }
    }
}

std::set<QPDFObjGen>
QPDFJob::getWantedJSONObjects()
{
    QPDFJob& o = *this; // QXXXQ
    std::set<QPDFObjGen> wanted_og;
    for (auto const& iter: o.json_objects)
    {
        bool trailer;
        int obj = 0;
        int gen = 0;
        QPDFJob::parse_object_id(iter, trailer, obj, gen);
        if (obj)
        {
            wanted_og.insert(QPDFObjGen(obj, gen));
        }
    }
    return wanted_og;
}

void
QPDFJob::doJSONObjects(QPDF& pdf, JSON& j)
{
    QPDFJob& o = *this; // QXXXQ
    // Add all objects. Do this first before other code below modifies
    // things by doing stuff like calling
    // pushInheritedAttributesToPage.
    bool all_objects = o.json_objects.empty();
    std::set<QPDFObjGen> wanted_og = getWantedJSONObjects();
    JSON j_objects = j.addDictionaryMember("objects", JSON::makeDictionary());
    if (all_objects || o.json_objects.count("trailer"))
    {
        j_objects.addDictionaryMember(
            "trailer", pdf.getTrailer().getJSON(true));
    }
    std::vector<QPDFObjectHandle> objects = pdf.getAllObjects();
    for (std::vector<QPDFObjectHandle>::iterator iter = objects.begin();
         iter != objects.end(); ++iter)
    {
        if (all_objects || wanted_og.count((*iter).getObjGen()))
        {
            j_objects.addDictionaryMember(
                (*iter).unparse(), (*iter).getJSON(true));
        }
    }
}

void
QPDFJob::doJSONObjectinfo(QPDF& pdf, JSON& j)
{
    QPDFJob& o = *this; // QXXXQ
    // Do this first before other code below modifies things by doing
    // stuff like calling pushInheritedAttributesToPage.
    bool all_objects = o.json_objects.empty();
    std::set<QPDFObjGen> wanted_og = getWantedJSONObjects();
    JSON j_objectinfo = j.addDictionaryMember(
        "objectinfo", JSON::makeDictionary());
    for (auto& obj: pdf.getAllObjects())
    {
        if (all_objects || wanted_og.count(obj.getObjGen()))
        {
            auto j_details = j_objectinfo.addDictionaryMember(
                obj.unparse(), JSON::makeDictionary());
            auto j_stream = j_details.addDictionaryMember(
                "stream", JSON::makeDictionary());
            bool is_stream = obj.isStream();
            j_stream.addDictionaryMember(
                "is", JSON::makeBool(is_stream));
            j_stream.addDictionaryMember(
                "length",
                (is_stream
                 ? obj.getDict().getKey("/Length").getJSON(true)
                 : JSON::makeNull()));
            j_stream.addDictionaryMember(
                "filter",
                (is_stream
                 ? obj.getDict().getKey("/Filter").getJSON(true)
                 : JSON::makeNull()));
        }
    }
}

void
QPDFJob::doJSONPages(QPDF& pdf, JSON& j)
{
    QPDFJob& o = *this; // QXXXQ
    JSON j_pages = j.addDictionaryMember("pages", JSON::makeArray());
    QPDFPageDocumentHelper pdh(pdf);
    QPDFPageLabelDocumentHelper pldh(pdf);
    QPDFOutlineDocumentHelper odh(pdf);
    pdh.pushInheritedAttributesToPage();
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    int pageno = 0;
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter, ++pageno)
    {
        JSON j_page = j_pages.addArrayElement(JSON::makeDictionary());
        QPDFPageObjectHelper& ph(*iter);
        QPDFObjectHandle page = ph.getObjectHandle();
        j_page.addDictionaryMember("object", page.getJSON());
        JSON j_images = j_page.addDictionaryMember(
            "images", JSON::makeArray());
        std::map<std::string, QPDFObjectHandle> images = ph.getImages();
        for (auto const& iter2: images)
        {
            JSON j_image = j_images.addArrayElement(JSON::makeDictionary());
            j_image.addDictionaryMember(
                "name", JSON::makeString(iter2.first));
            QPDFObjectHandle image = iter2.second;
            QPDFObjectHandle dict = image.getDict();
            j_image.addDictionaryMember("object", image.getJSON());
            j_image.addDictionaryMember(
                "width", dict.getKey("/Width").getJSON());
            j_image.addDictionaryMember(
                "height", dict.getKey("/Height").getJSON());
            j_image.addDictionaryMember(
                "colorspace", dict.getKey("/ColorSpace").getJSON());
            j_image.addDictionaryMember(
                "bitspercomponent", dict.getKey("/BitsPerComponent").getJSON());
            QPDFObjectHandle filters = dict.getKey("/Filter").wrapInArray();
            j_image.addDictionaryMember(
                "filter", filters.getJSON());
            QPDFObjectHandle decode_parms = dict.getKey("/DecodeParms");
            QPDFObjectHandle dp_array;
            if (decode_parms.isArray())
            {
                dp_array = decode_parms;
            }
            else
            {
                dp_array = QPDFObjectHandle::newArray();
                for (int i = 0; i < filters.getArrayNItems(); ++i)
                {
                    dp_array.appendItem(decode_parms);
                }
            }
            j_image.addDictionaryMember("decodeparms", dp_array.getJSON());
            j_image.addDictionaryMember(
                "filterable",
                JSON::makeBool(
                    image.pipeStreamData(0, 0, o.decode_level, true)));
        }
        j_page.addDictionaryMember("images", j_images);
        JSON j_contents = j_page.addDictionaryMember(
            "contents", JSON::makeArray());
        std::vector<QPDFObjectHandle> content = ph.getPageContents();
        for (auto& iter2: content)
        {
            j_contents.addArrayElement(iter2.getJSON());
        }
        j_page.addDictionaryMember(
            "label", pldh.getLabelForPage(pageno).getJSON());
        JSON j_outlines = j_page.addDictionaryMember(
            "outlines", JSON::makeArray());
        std::vector<QPDFOutlineObjectHelper> outlines =
            odh.getOutlinesForPage(page.getObjGen());
        for (std::vector<QPDFOutlineObjectHelper>::iterator oiter =
                 outlines.begin();
             oiter != outlines.end(); ++oiter)
        {
            JSON j_outline = j_outlines.addArrayElement(JSON::makeDictionary());
            j_outline.addDictionaryMember(
                "object", (*oiter).getObjectHandle().getJSON());
            j_outline.addDictionaryMember(
                "title", JSON::makeString((*oiter).getTitle()));
            j_outline.addDictionaryMember(
                "dest", (*oiter).getDest().getJSON(true));
        }
        j_page.addDictionaryMember("pageposfrom1", JSON::makeInt(1 + pageno));
    }
}

void
QPDFJob::doJSONPageLabels(QPDF& pdf, JSON& j)
{
    JSON j_labels = j.addDictionaryMember("pagelabels", JSON::makeArray());
    QPDFPageLabelDocumentHelper pldh(pdf);
    QPDFPageDocumentHelper pdh(pdf);
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    if (pldh.hasPageLabels())
    {
        std::vector<QPDFObjectHandle> labels;
        pldh.getLabelsForPageRange(
            0, QIntC::to_int(pages.size()) - 1, 0, labels);
        for (std::vector<QPDFObjectHandle>::iterator iter = labels.begin();
             iter != labels.end(); ++iter)
        {
            std::vector<QPDFObjectHandle>::iterator next = iter;
            ++next;
            if (next == labels.end())
            {
                // This can't happen, so ignore it. This could only
                // happen if getLabelsForPageRange somehow returned an
                // odd number of items.
                break;
            }
            JSON j_label = j_labels.addArrayElement(JSON::makeDictionary());
            j_label.addDictionaryMember("index", (*iter).getJSON());
            ++iter;
            j_label.addDictionaryMember("label", (*iter).getJSON());
        }
    }
}

static void add_outlines_to_json(
    std::vector<QPDFOutlineObjectHelper> outlines, JSON& j,
    std::map<QPDFObjGen, int>& page_numbers)
{
    for (std::vector<QPDFOutlineObjectHelper>::iterator iter = outlines.begin();
         iter != outlines.end(); ++iter)
    {
        QPDFOutlineObjectHelper& ol = *iter;
        JSON jo = j.addArrayElement(JSON::makeDictionary());
        jo.addDictionaryMember("object", ol.getObjectHandle().getJSON());
        jo.addDictionaryMember("title", JSON::makeString(ol.getTitle()));
        jo.addDictionaryMember("dest", ol.getDest().getJSON(true));
        jo.addDictionaryMember("open", JSON::makeBool(ol.getCount() >= 0));
        QPDFObjectHandle page = ol.getDestPage();
        JSON j_destpage = JSON::makeNull();
        if (page.isIndirect())
        {
            QPDFObjGen og = page.getObjGen();
            if (page_numbers.count(og))
            {
                j_destpage = JSON::makeInt(page_numbers[og]);
            }
        }
        jo.addDictionaryMember("destpageposfrom1", j_destpage);
        JSON j_kids = jo.addDictionaryMember("kids", JSON::makeArray());
        add_outlines_to_json(ol.getKids(), j_kids, page_numbers);
    }
}

void
QPDFJob::doJSONOutlines(QPDF& pdf, JSON& j)
{
    std::map<QPDFObjGen, int> page_numbers;
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    int n = 0;
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFObjectHandle oh = (*iter).getObjectHandle();
        page_numbers[oh.getObjGen()] = ++n;
    }

    JSON j_outlines = j.addDictionaryMember(
        "outlines", JSON::makeArray());
    QPDFOutlineDocumentHelper odh(pdf);
    add_outlines_to_json(odh.getTopLevelOutlines(), j_outlines, page_numbers);
}

void
QPDFJob::doJSONAcroform(QPDF& pdf, JSON& j)
{
    JSON j_acroform = j.addDictionaryMember(
        "acroform", JSON::makeDictionary());
    QPDFAcroFormDocumentHelper afdh(pdf);
    j_acroform.addDictionaryMember(
        "hasacroform",
        JSON::makeBool(afdh.hasAcroForm()));
    j_acroform.addDictionaryMember(
        "needappearances",
        JSON::makeBool(afdh.getNeedAppearances()));
    JSON j_fields = j_acroform.addDictionaryMember(
        "fields", JSON::makeArray());
    QPDFPageDocumentHelper pdh(pdf);
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    int pagepos1 = 0;
    for (std::vector<QPDFPageObjectHelper>::iterator page_iter =
             pages.begin();
         page_iter != pages.end(); ++page_iter)
    {
        ++pagepos1;
        std::vector<QPDFAnnotationObjectHelper> annotations =
            afdh.getWidgetAnnotationsForPage(*page_iter);
        for (std::vector<QPDFAnnotationObjectHelper>::iterator annot_iter =
                 annotations.begin();
             annot_iter != annotations.end(); ++annot_iter)
        {
            QPDFAnnotationObjectHelper& aoh = *annot_iter;
            QPDFFormFieldObjectHelper ffh =
                afdh.getFieldForAnnotation(aoh);
            JSON j_field = j_fields.addArrayElement(
                JSON::makeDictionary());
            j_field.addDictionaryMember(
                "object",
                ffh.getObjectHandle().getJSON());
            j_field.addDictionaryMember(
                "parent",
                ffh.getObjectHandle().getKey("/Parent").getJSON());
            j_field.addDictionaryMember(
                "pageposfrom1",
                JSON::makeInt(pagepos1));
            j_field.addDictionaryMember(
                "fieldtype",
                JSON::makeString(ffh.getFieldType()));
            j_field.addDictionaryMember(
                "fieldflags",
                JSON::makeInt(ffh.getFlags()));
            j_field.addDictionaryMember(
                "fullname",
                JSON::makeString(ffh.getFullyQualifiedName()));
            j_field.addDictionaryMember(
                "partialname",
                JSON::makeString(ffh.getPartialName()));
            j_field.addDictionaryMember(
                "alternativename",
                JSON::makeString(ffh.getAlternativeName()));
            j_field.addDictionaryMember(
                "mappingname",
                JSON::makeString(ffh.getMappingName()));
            j_field.addDictionaryMember(
                "value",
                ffh.getValue().getJSON());
            j_field.addDictionaryMember(
                "defaultvalue",
                ffh.getDefaultValue().getJSON());
            j_field.addDictionaryMember(
                "quadding",
                JSON::makeInt(ffh.getQuadding()));
            j_field.addDictionaryMember(
                "ischeckbox",
                JSON::makeBool(ffh.isCheckbox()));
            j_field.addDictionaryMember(
                "isradiobutton",
                JSON::makeBool(ffh.isRadioButton()));
            j_field.addDictionaryMember(
                "ischoice",
                JSON::makeBool(ffh.isChoice()));
            j_field.addDictionaryMember(
                "istext",
                JSON::makeBool(ffh.isText()));
            JSON j_choices = j_field.addDictionaryMember(
                "choices", JSON::makeArray());
            std::vector<std::string> choices = ffh.getChoices();
            for (std::vector<std::string>::iterator iter = choices.begin();
                 iter != choices.end(); ++iter)
            {
                j_choices.addArrayElement(JSON::makeString(*iter));
            }
            JSON j_annot = j_field.addDictionaryMember(
                "annotation", JSON::makeDictionary());
            j_annot.addDictionaryMember(
                "object",
                aoh.getObjectHandle().getJSON());
            j_annot.addDictionaryMember(
                "appearancestate",
                JSON::makeString(aoh.getAppearanceState()));
            j_annot.addDictionaryMember(
                "annotationflags",
                JSON::makeInt(aoh.getFlags()));
        }
    }
}

void
QPDFJob::doJSONEncrypt(QPDF& pdf, JSON& j)
{
    QPDFJob& o = *this; // QXXXQ
    int R = 0;
    int P = 0;
    int V = 0;
    QPDF::encryption_method_e stream_method = QPDF::e_none;
    QPDF::encryption_method_e string_method = QPDF::e_none;
    QPDF::encryption_method_e file_method = QPDF::e_none;
    bool is_encrypted = pdf.isEncrypted(
        R, P, V, stream_method, string_method, file_method);
    JSON j_encrypt = j.addDictionaryMember(
        "encrypt", JSON::makeDictionary());
    j_encrypt.addDictionaryMember(
        "encrypted",
        JSON::makeBool(is_encrypted));
    j_encrypt.addDictionaryMember(
        "userpasswordmatched",
        JSON::makeBool(is_encrypted && pdf.userPasswordMatched()));
    j_encrypt.addDictionaryMember(
        "ownerpasswordmatched",
        JSON::makeBool(is_encrypted && pdf.ownerPasswordMatched()));
    JSON j_capabilities = j_encrypt.addDictionaryMember(
        "capabilities", JSON::makeDictionary());
    j_capabilities.addDictionaryMember(
        "accessibility",
        JSON::makeBool(pdf.allowAccessibility()));
    j_capabilities.addDictionaryMember(
        "extract",
        JSON::makeBool(pdf.allowExtractAll()));
    j_capabilities.addDictionaryMember(
        "printlow",
        JSON::makeBool(pdf.allowPrintLowRes()));
    j_capabilities.addDictionaryMember(
        "printhigh",
        JSON::makeBool(pdf.allowPrintHighRes()));
    j_capabilities.addDictionaryMember(
        "modifyassembly",
        JSON::makeBool(pdf.allowModifyAssembly()));
    j_capabilities.addDictionaryMember(
        "modifyforms",
        JSON::makeBool(pdf.allowModifyForm()));
    j_capabilities.addDictionaryMember(
        "moddifyannotations",
        JSON::makeBool(pdf.allowModifyAnnotation()));
    j_capabilities.addDictionaryMember(
        "modifyother",
        JSON::makeBool(pdf.allowModifyOther()));
    j_capabilities.addDictionaryMember(
        "modify",
        JSON::makeBool(pdf.allowModifyAll()));
    JSON j_parameters = j_encrypt.addDictionaryMember(
        "parameters", JSON::makeDictionary());
    j_parameters.addDictionaryMember("R", JSON::makeInt(R));
    j_parameters.addDictionaryMember("V", JSON::makeInt(V));
    j_parameters.addDictionaryMember("P", JSON::makeInt(P));
    int bits = 0;
    JSON key = JSON::makeNull();
    if (is_encrypted)
    {
        std::string encryption_key = pdf.getEncryptionKey();
        bits = QIntC::to_int(encryption_key.length() * 8);
        if (o.show_encryption_key)
        {
            key = JSON::makeString(QUtil::hex_encode(encryption_key));
        }
    }
    j_parameters.addDictionaryMember("bits", JSON::makeInt(bits));
    j_parameters.addDictionaryMember("key", key);
    auto fix_method = [is_encrypted](QPDF::encryption_method_e& m) {
                          if (is_encrypted && m == QPDF::e_none)
                          {
                              m = QPDF::e_rc4;
                          }
                      };
    fix_method(stream_method);
    fix_method(string_method);
    fix_method(file_method);
    std::string s_stream_method = show_encryption_method(stream_method);
    std::string s_string_method = show_encryption_method(string_method);
    std::string s_file_method = show_encryption_method(file_method);
    std::string s_overall_method;
    if ((stream_method == string_method) &&
        (stream_method == file_method))
    {
        s_overall_method = s_stream_method;
    }
    else
    {
        s_overall_method = "mixed";
    }
    j_parameters.addDictionaryMember(
        "method", JSON::makeString(s_overall_method));
    j_parameters.addDictionaryMember(
        "streammethod", JSON::makeString(s_stream_method));
    j_parameters.addDictionaryMember(
        "stringmethod", JSON::makeString(s_string_method));
    j_parameters.addDictionaryMember(
        "filemethod", JSON::makeString(s_file_method));
}

void
QPDFJob::doJSONAttachments(QPDF& pdf, JSON& j)
{
    JSON j_attachments = j.addDictionaryMember(
        "attachments", JSON::makeDictionary());
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    for (auto const& iter: efdh.getEmbeddedFiles())
    {
        std::string const& key = iter.first;
        auto fsoh = iter.second;
        auto j_details = j_attachments.addDictionaryMember(
            key, JSON::makeDictionary());
        j_details.addDictionaryMember(
            "filespec",
            JSON::makeString(fsoh->getObjectHandle().unparse()));
        j_details.addDictionaryMember(
            "preferredname", JSON::makeString(fsoh->getFilename()));
        j_details.addDictionaryMember(
            "preferredcontents",
            JSON::makeString(fsoh->getEmbeddedFileStream().unparse()));
    }
}

JSON
QPDFJob::json_schema(std::set<std::string>* keys)
{
    // Style: use all lower-case keys with no dashes or underscores.
    // Choose array or dictionary based on indexing. For example, we
    // use a dictionary for objects because we want to index by object
    // ID and an array for pages because we want to index by position.
    // The pages in the pages array contain references back to the
    // original object, which can be resolved in the objects
    // dictionary. When a PDF construct that maps back to an original
    // object is represented separately, use "object" as the key that
    // references the original object.

    // This JSON object doubles as a schema and as documentation for
    // our JSON output. Any schema mismatch is a bug in qpdf. This
    // helps to enforce our policy of consistently providing a known
    // structure where every documented key will always be present,
    // which makes it easier to consume our JSON. This is discussed in
    // more depth in the manual.
    JSON schema = JSON::makeDictionary();
    schema.addDictionaryMember(
        "version", JSON::makeString(
            "JSON format serial number; increased for non-compatible changes"));
    JSON j_params = schema.addDictionaryMember(
        "parameters", JSON::makeDictionary());
    j_params.addDictionaryMember(
        "decodelevel", JSON::makeString(
            "decode level used to determine stream filterability"));

    bool all_keys = ((keys == 0) || keys->empty());

    // QXXXQ
    // The list of selectable top-level keys id duplicated in three
    // places: json_schema, doJSON, and initOptionTable.
    if (all_keys || keys->count("objects"))
    {
        schema.addDictionaryMember(
            "objects", JSON::makeString(
                "dictionary of original objects;"
                " keys are 'trailer' or 'n n R'"));
    }
    if (all_keys || keys->count("objectinfo"))
    {
        JSON objectinfo = schema.addDictionaryMember(
            "objectinfo", JSON::makeDictionary());
        JSON details = objectinfo.addDictionaryMember(
            "<object-id>", JSON::makeDictionary());
        JSON stream = details.addDictionaryMember(
            "stream", JSON::makeDictionary());
        stream.addDictionaryMember(
            "is",
            JSON::makeString("whether the object is a stream"));
        stream.addDictionaryMember(
            "length",
            JSON::makeString("if stream, its length, otherwise null"));
        stream.addDictionaryMember(
            "filter",
            JSON::makeString("if stream, its filters, otherwise null"));
    }
    if (all_keys || keys->count("pages"))
    {
        JSON page = schema.addDictionaryMember("pages", JSON::makeArray()).
            addArrayElement(JSON::makeDictionary());
        page.addDictionaryMember(
            "object",
            JSON::makeString("reference to original page object"));
        JSON image = page.addDictionaryMember("images", JSON::makeArray()).
            addArrayElement(JSON::makeDictionary());
        image.addDictionaryMember(
            "name",
            JSON::makeString("name of image in XObject table"));
        image.addDictionaryMember(
            "object",
            JSON::makeString("reference to image stream"));
        image.addDictionaryMember(
            "width",
            JSON::makeString("image width"));
        image.addDictionaryMember(
            "height",
            JSON::makeString("image height"));
        image.addDictionaryMember(
            "colorspace",
            JSON::makeString("color space"));
        image.addDictionaryMember(
            "bitspercomponent",
            JSON::makeString("bits per component"));
        image.addDictionaryMember("filter", JSON::makeArray()).
            addArrayElement(
                JSON::makeString("filters applied to image data"));
        image.addDictionaryMember("decodeparms", JSON::makeArray()).
            addArrayElement(
                JSON::makeString("decode parameters for image data"));
        image.addDictionaryMember(
            "filterable",
            JSON::makeString("whether image data can be decoded"
                             " using the decode level qpdf was invoked with"));
        page.addDictionaryMember("contents", JSON::makeArray()).
            addArrayElement(
                JSON::makeString("reference to each content stream"));
        page.addDictionaryMember(
            "label",
            JSON::makeString("page label dictionary, or null if none"));
        JSON outline = page.addDictionaryMember("outlines", JSON::makeArray()).
            addArrayElement(JSON::makeDictionary());
        outline.addDictionaryMember(
            "object",
            JSON::makeString("reference to outline that targets this page"));
        outline.addDictionaryMember(
            "title",
            JSON::makeString("outline title"));
        outline.addDictionaryMember(
            "dest",
            JSON::makeString("outline destination dictionary"));
        page.addDictionaryMember(
            "pageposfrom1",
            JSON::makeString("position of page in document numbering from 1"));
    }
    if (all_keys || keys->count("pagelabels"))
    {
        JSON labels = schema.addDictionaryMember(
            "pagelabels", JSON::makeArray()).
            addArrayElement(JSON::makeDictionary());
        labels.addDictionaryMember(
            "index",
            JSON::makeString("starting page position starting from zero"));
        labels.addDictionaryMember(
            "label",
            JSON::makeString("page label dictionary"));
    }
    if (all_keys || keys->count("outlines"))
    {
        JSON outlines = schema.addDictionaryMember(
            "outlines", JSON::makeArray()).
            addArrayElement(JSON::makeDictionary());
        outlines.addDictionaryMember(
            "object",
            JSON::makeString("reference to this outline"));
        outlines.addDictionaryMember(
            "title",
            JSON::makeString("outline title"));
        outlines.addDictionaryMember(
            "dest",
            JSON::makeString("outline destination dictionary"));
        outlines.addDictionaryMember(
            "kids",
            JSON::makeString("array of descendent outlines"));
        outlines.addDictionaryMember(
            "open",
            JSON::makeString("whether the outline is displayed expanded"));
        outlines.addDictionaryMember(
            "destpageposfrom1",
            JSON::makeString("position of destination page in document"
                             " numbered from 1; null if not known"));
    }
    if (all_keys || keys->count("acroform"))
    {
        JSON acroform = schema.addDictionaryMember(
            "acroform", JSON::makeDictionary());
        acroform.addDictionaryMember(
            "hasacroform",
            JSON::makeString("whether the document has interactive forms"));
        acroform.addDictionaryMember(
            "needappearances",
            JSON::makeString("whether the form fields' appearance"
                             " streams need to be regenerated"));
        JSON fields = acroform.addDictionaryMember(
            "fields", JSON::makeArray()).
            addArrayElement(JSON::makeDictionary());
        fields.addDictionaryMember(
            "object",
            JSON::makeString("reference to this form field"));
        fields.addDictionaryMember(
            "parent",
            JSON::makeString("reference to this field's parent"));
        fields.addDictionaryMember(
            "pageposfrom1",
            JSON::makeString("position of containing page numbered from 1"));
        fields.addDictionaryMember(
            "fieldtype",
            JSON::makeString("field type"));
        fields.addDictionaryMember(
            "fieldflags",
            JSON::makeString(
                "form field flags from /Ff --"
                " see pdf_form_field_flag_e in qpdf/Constants.h"));
        fields.addDictionaryMember(
            "fullname",
            JSON::makeString("full name of field"));
        fields.addDictionaryMember(
            "partialname",
            JSON::makeString("partial name of field"));
        fields.addDictionaryMember(
            "alternativename",
            JSON::makeString(
                "alternative name of field --"
                " this is the one usually shown to users"));
        fields.addDictionaryMember(
            "mappingname",
            JSON::makeString("mapping name of field"));
        fields.addDictionaryMember(
            "value",
            JSON::makeString("value of field"));
        fields.addDictionaryMember(
            "defaultvalue",
            JSON::makeString("default value of field"));
        fields.addDictionaryMember(
            "quadding",
            JSON::makeString(
                "field quadding --"
                " number indicating left, center, or right"));
        fields.addDictionaryMember(
            "ischeckbox",
            JSON::makeString("whether field is a checkbox"));
        fields.addDictionaryMember(
            "isradiobutton",
            JSON::makeString("whether field is a radio button --"
                             " buttons in a single group share a parent"));
        fields.addDictionaryMember(
            "ischoice",
            JSON::makeString("whether field is a list, combo, or dropdown"));
        fields.addDictionaryMember(
            "istext",
            JSON::makeString("whether field is a text field"));
        JSON j_choices = fields.addDictionaryMember(
            "choices",
            JSON::makeString("for choices fields, the list of"
                             " choices presented to the user"));
        JSON annotation = fields.addDictionaryMember(
            "annotation", JSON::makeDictionary());
        annotation.addDictionaryMember(
            "object",
            JSON::makeString("reference to the annotation object"));
        annotation.addDictionaryMember(
            "appearancestate",
            JSON::makeString("appearance state --"
                             " can be used to determine value for"
                             " checkboxes and radio buttons"));
        annotation.addDictionaryMember(
            "annotationflags",
            JSON::makeString(
                "annotation flags from /F --"
                " see pdf_annotation_flag_e in qpdf/Constants.h"));
    }
    if (all_keys || keys->count("encrypt"))
    {
        JSON encrypt = schema.addDictionaryMember(
            "encrypt", JSON::makeDictionary());
        encrypt.addDictionaryMember(
            "encrypted",
            JSON::makeString("whether the document is encrypted"));
        encrypt.addDictionaryMember(
            "userpasswordmatched",
            JSON::makeString("whether supplied password matched user password;"
                             " always false for non-encrypted files"));
        encrypt.addDictionaryMember(
            "ownerpasswordmatched",
            JSON::makeString("whether supplied password matched owner password;"
                             " always false for non-encrypted files"));
        JSON capabilities = encrypt.addDictionaryMember(
            "capabilities", JSON::makeDictionary());
        capabilities.addDictionaryMember(
            "accessibility",
            JSON::makeString("allow extraction for accessibility?"));
        capabilities.addDictionaryMember(
            "extract",
            JSON::makeString("allow extraction?"));
        capabilities.addDictionaryMember(
            "printlow",
            JSON::makeString("allow low resolution printing?"));
        capabilities.addDictionaryMember(
            "printhigh",
            JSON::makeString("allow high resolution printing?"));
        capabilities.addDictionaryMember(
            "modifyassembly",
            JSON::makeString("allow modifying document assembly?"));
        capabilities.addDictionaryMember(
            "modifyforms",
            JSON::makeString("allow modifying forms?"));
        capabilities.addDictionaryMember(
            "moddifyannotations",
            JSON::makeString("allow modifying annotations?"));
        capabilities.addDictionaryMember(
            "modifyother",
            JSON::makeString("allow other modifications?"));
        capabilities.addDictionaryMember(
            "modify",
            JSON::makeString("allow all modifications?"));

        JSON parameters = encrypt.addDictionaryMember(
            "parameters", JSON::makeDictionary());
        parameters.addDictionaryMember(
            "R",
            JSON::makeString("R value from Encrypt dictionary"));
        parameters.addDictionaryMember(
            "V",
            JSON::makeString("V value from Encrypt dictionary"));
        parameters.addDictionaryMember(
            "P",
            JSON::makeString("P value from Encrypt dictionary"));
        parameters.addDictionaryMember(
            "bits",
            JSON::makeString("encryption key bit length"));
        parameters.addDictionaryMember(
            "key",
            JSON::makeString("encryption key; will be null"
                             " unless --show-encryption-key was specified"));
        parameters.addDictionaryMember(
            "method",
            JSON::makeString("overall encryption method:"
                             " none, mixed, RC4, AESv2, AESv3"));
        parameters.addDictionaryMember(
            "streammethod",
            JSON::makeString("encryption method for streams"));
        parameters.addDictionaryMember(
            "stringmethod",
            JSON::makeString("encryption method for string"));
        parameters.addDictionaryMember(
            "filemethod",
            JSON::makeString("encryption method for attachments"));
    }
    if (all_keys || keys->count("attachments"))
    {
        JSON attachments = schema.addDictionaryMember(
            "attachments", JSON::makeDictionary());
        JSON details = attachments.addDictionaryMember(
            "<attachment-key>", JSON::makeDictionary());
        details.addDictionaryMember(
            "filespec",
            JSON::makeString("object containing the file spec"));
        details.addDictionaryMember(
            "preferredname",
            JSON::makeString("most preferred file name"));
        details.addDictionaryMember(
            "preferredcontents",
            JSON::makeString("most preferred embedded file stream"));
    }
    return schema;
}

void
QPDFJob::doJSON(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    JSON j = JSON::makeDictionary();
    // This version is updated every time a non-backward-compatible
    // change is made to the JSON format. Clients of the JSON are to
    // ignore unrecognized keys, so we only update the version of a
    // key disappears or if its value changes meaning.
    j.addDictionaryMember("version", JSON::makeInt(1));
    JSON j_params = j.addDictionaryMember(
        "parameters", JSON::makeDictionary());
    std::string decode_level_str;
    switch (o.decode_level)
    {
        case qpdf_dl_none:
          decode_level_str = "none";
          break;
        case qpdf_dl_generalized:
          decode_level_str = "generalized";
          break;
        case qpdf_dl_specialized:
          decode_level_str = "specialized";
          break;
        case qpdf_dl_all:
          decode_level_str = "all";
          break;
    }
    j_params.addDictionaryMember(
        "decodelevel", JSON::makeString(decode_level_str));

    bool all_keys = o.json_keys.empty();
    // QXXXQ
    // The list of selectable top-level keys id duplicated in three
    // places: json_schema, doJSON, and initOptionTable.
    if (all_keys || o.json_keys.count("objects"))
    {
        doJSONObjects(pdf, j);
    }
    if (all_keys || o.json_keys.count("objectinfo"))
    {
        doJSONObjectinfo(pdf, j);
    }
    if (all_keys || o.json_keys.count("pages"))
    {
        doJSONPages(pdf, j);
    }
    if (all_keys || o.json_keys.count("pagelabels"))
    {
        doJSONPageLabels(pdf, j);
    }
    if (all_keys || o.json_keys.count("outlines"))
    {
        doJSONOutlines(pdf, j);
    }
    if (all_keys || o.json_keys.count("acroform"))
    {
        doJSONAcroform(pdf, j);
    }
    if (all_keys || o.json_keys.count("encrypt"))
    {
        doJSONEncrypt(pdf, j);
    }
    if (all_keys || o.json_keys.count("attachments"))
    {
        doJSONAttachments(pdf, j);
    }

    // Check against schema

    JSON schema = QPDFJob::json_schema(&o.json_keys);
    std::list<std::string> errors;
    if (! j.checkSchema(schema, errors))
    {
        *(this->m->cerr)
            << "QPDFJob didn't create JSON that complies with its own rules.\n\
Please report this as a bug at\n\
   https://github.com/qpdf/qpdf/issues/new\n\
ideally with the file that caused the error and the output below. Thanks!\n\
\n";
        for (std::list<std::string>::iterator iter = errors.begin();
             iter != errors.end(); ++iter)
        {
            *(this->m->cerr) << (*iter) << std::endl;
        }
    }

    *(this->m->cout) << j.unparse() << std::endl;
}

void
QPDFJob::doInspection(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    if (o.check)
    {
        doCheck(pdf);
    }
    if (o.json)
    {
        doJSON(pdf);
    }
    if (o.show_npages)
    {
        QTC::TC("qpdf", "qpdf npages");
        *(this->m->cout) << pdf.getRoot().getKey("/Pages").
            getKey("/Count").getIntValue() << std::endl;
    }
    if (o.show_encryption)
    {
        showEncryption(pdf);
    }
    if (o.check_linearization)
    {
        if (pdf.checkLinearization())
        {
            *(this->m->cout)
                << o.infilename << ": no linearization errors" << std::endl;
        }
        else
        {
            this->m->warnings = true;
        }
    }
    if (o.show_linearization)
    {
        if (pdf.isLinearized())
        {
            pdf.showLinearizationData();
        }
        else
        {
            *(this->m->cout)
                << o.infilename << " is not linearized" << std::endl;
        }
    }
    if (o.show_xref)
    {
        pdf.showXRefTable();
    }
    if ((o.show_obj > 0) || o.show_trailer)
    {
        doShowObj(pdf);
    }
    if (o.show_pages)
    {
        doShowPages(pdf);
    }
    if (o.list_attachments)
    {
        doListAttachments(pdf);
    }
    if (! o.attachment_to_show.empty())
    {
        doShowAttachment(pdf);
    }
    if (! pdf.getWarnings().empty())
    {
        this->m->warnings = true;
    }
}

PointerHolder<QPDF>
QPDFJob::doProcessOnce(
    std::function<void(QPDF*, char const*)> fn,
    char const* password, bool empty)
{
    PointerHolder<QPDF> pdf = new QPDF;
    setQPDFOptions(*pdf);
    if (empty)
    {
        pdf->emptyPDF();
    }
    else
    {
        fn(pdf.getPointer(), password);
    }
    return pdf;
}

PointerHolder<QPDF>
QPDFJob::doProcess(
    std::function<void(QPDF*, char const*)> fn,
    char const* password, bool empty)
{
    QPDFJob& o = *this; // QXXXQ
    // If a password has been specified but doesn't work, try other
    // passwords that are equivalent in different character encodings.
    // This makes it possible to open PDF files that were encrypted
    // using incorrect string encodings. For example, if someone used
    // a password encoded in PDF Doc encoding or Windows code page
    // 1252 for an AES-encrypted file or a UTF-8-encoded password on
    // an RC4-encrypted file, or if the password was properly encoded
    // by the password given here was incorrectly encoded, there's a
    // good chance we'd succeed here.

    std::string ptemp;
    if (password && (! o.password_is_hex_key))
    {
        if (o.password_mode == QPDFJob::pm_hex_bytes)
        {
            // Special case: handle --password-mode=hex-bytes for input
            // password as well as output password
            QTC::TC("qpdf", "qpdf input password hex-bytes");
            ptemp = QUtil::hex_decode(password);
            password = ptemp.c_str();
        }
    }
    if ((password == 0) || empty || o.password_is_hex_key ||
        o.suppress_password_recovery)
    {
        // There is no password, or we're not doing recovery, so just
        // do the normal processing with the supplied password.
        return doProcessOnce(fn, password, empty);
    }

    // Get a list of otherwise encoded strings. Keep in scope for this
    // method.
    std::vector<std::string> passwords_str =
        QUtil::possible_repaired_encodings(password);
    // Represent to char const*, as required by the QPDF class.
    std::vector<char const*> passwords;
    for (std::vector<std::string>::iterator iter = passwords_str.begin();
         iter != passwords_str.end(); ++iter)
    {
        passwords.push_back((*iter).c_str());
    }
    // We always try the supplied password first because it is the
    // first string returned by possible_repaired_encodings. If there
    // is more than one option, go ahead and put the supplied password
    // at the end so that it's that decoding attempt whose exception
    // is thrown.
    if (passwords.size() > 1)
    {
        passwords.push_back(password);
    }

    // Try each password. If one works, return the resulting object.
    // If they all fail, throw the exception thrown by the final
    // attempt, which, like the first attempt, will be with the
    // supplied password.
    bool warned = false;
    for (std::vector<char const*>::iterator iter = passwords.begin();
         iter != passwords.end(); ++iter)
    {
        try
        {
            return doProcessOnce(fn, *iter, empty);
        }
        catch (QPDFExc& e)
        {
            std::vector<char const*>::iterator next = iter;
            ++next;
            if (next == passwords.end())
            {
                throw e;
            }
        }
        if (! warned)
        {
            warned = true;
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": supplied password didn't work;"
                     << " trying other passwords based on interpreting"
                     << " password with different string encodings"
                     << std::endl;
            });
        }
    }
    // Should not be reachable
    throw std::logic_error("do_process returned");
}

PointerHolder<QPDF>
QPDFJob::processFile(char const* filename, char const* password)
{
    auto f1 = std::mem_fn<void(char const*, char const*)>(&QPDF::processFile);
    auto fn = std::bind(
        f1, std::placeholders::_1, filename, std::placeholders::_2);
    return doProcess(fn, password, strcmp(filename, "") == 0);
}

PointerHolder<QPDF>
QPDFJob::processInputSource(
    PointerHolder<InputSource> is, char const* password)
{
    auto f1 = std::mem_fn(&QPDF::processInputSource);
    auto fn = std::bind(
        f1, std::placeholders::_1, is, std::placeholders::_2);
    return doProcess(fn, password, false);
}

void
QPDFJob::validateUnderOverlay(QPDF& pdf, QPDFJob::UnderOverlay* uo)
{
    if (0 == uo->filename)
    {
        return;
    }
    QPDFPageDocumentHelper main_pdh(pdf);
    int main_npages = QIntC::to_int(main_pdh.getAllPages().size());
    uo->pdf = processFile(uo->filename, uo->password);
    QPDFPageDocumentHelper uo_pdh(*(uo->pdf));
    int uo_npages = QIntC::to_int(uo_pdh.getAllPages().size());
    try
    {
        uo->to_pagenos = QUtil::parse_numrange(uo->to_nr, main_npages);
    }
    catch (std::runtime_error& e)
    {
        throw std::runtime_error(
            "parsing numeric range for " + uo->which +
            " \"to\" pages: " + e.what());
    }
    try
    {
        if (0 == strlen(uo->from_nr))
        {
            QTC::TC("qpdf", "qpdf from_nr from repeat_nr");
            uo->from_nr = uo->repeat_nr;
        }
        uo->from_pagenos = QUtil::parse_numrange(uo->from_nr, uo_npages);
        if (strlen(uo->repeat_nr))
        {
            uo->repeat_pagenos =
                QUtil::parse_numrange(uo->repeat_nr, uo_npages);
        }
    }
    catch (std::runtime_error& e)
    {
        throw std::runtime_error(
            "parsing numeric range for " + uo->which + " file " +
            uo->filename + ": " + e.what());
    }
}

static QPDFAcroFormDocumentHelper* get_afdh_for_qpdf(
    std::map<unsigned long long,
             PointerHolder<QPDFAcroFormDocumentHelper>>& afdh_map,
    QPDF* q)
{
    auto uid = q->getUniqueId();
    if (! afdh_map.count(uid))
    {
        afdh_map[uid] = new QPDFAcroFormDocumentHelper(*q);
    }
    return afdh_map[uid].getPointer();
}

void
QPDFJob::doUnderOverlayForPage(
    QPDF& pdf,
    QPDFJob::UnderOverlay& uo,
    std::map<int, std::vector<int> >& pagenos,
    size_t page_idx,
    std::map<int, QPDFObjectHandle>& fo,
    std::vector<QPDFPageObjectHelper>& pages,
    QPDFPageObjectHelper& dest_page,
    bool before)
{
    QPDFJob& o = *this; // QXXXQ
    int pageno = 1 + QIntC::to_int(page_idx);
    if (! pagenos.count(pageno))
    {
        return;
    }

    std::map<unsigned long long,
             PointerHolder<QPDFAcroFormDocumentHelper>> afdh;
    auto make_afdh = [&](QPDFPageObjectHelper& ph) {
        QPDF* q = ph.getObjectHandle().getOwningQPDF();
        return get_afdh_for_qpdf(afdh, q);
    };
    auto dest_afdh = make_afdh(dest_page);

    std::string content;
    int min_suffix = 1;
    QPDFObjectHandle resources = dest_page.getAttribute("/Resources", true);
    if (! resources.isDictionary())
    {
        QTC::TC("qpdf", "qpdf overlay page with no resources");
        resources = QPDFObjectHandle::newDictionary();
        dest_page.getObjectHandle().replaceKey("/Resources", resources);
    }
    for (std::vector<int>::iterator iter = pagenos[pageno].begin();
         iter != pagenos[pageno].end(); ++iter)
    {
        int from_pageno = *iter;
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << "    " << uo.which << " " << from_pageno << std::endl;
        });
        auto from_page = pages.at(QIntC::to_size(from_pageno - 1));
        if (0 == fo.count(from_pageno))
        {
            fo[from_pageno] =
                pdf.copyForeignObject(
                    from_page.getFormXObjectForPage());
        }

        // If the same page is overlaid or underlaid multiple times,
        // we'll generate multiple names for it, but that's harmless
        // and also a pretty goofy case that's not worth coding
        // around.
        std::string name = resources.getUniqueResourceName("/Fx", min_suffix);
        QPDFMatrix cm;
        std::string new_content = dest_page.placeFormXObject(
            fo[from_pageno], name,
            dest_page.getTrimBox().getArrayAsRectangle(), cm);
        dest_page.copyAnnotations(
            from_page, cm, dest_afdh, make_afdh(from_page));
        if (! new_content.empty())
        {
            resources.mergeResources(
                QPDFObjectHandle::parse("<< /XObject << >> >>"));
            auto xobject = resources.getKey("/XObject");
            if (xobject.isDictionary())
            {
                xobject.replaceKey(name, fo[from_pageno]);
            }
            ++min_suffix;
            content += new_content;
        }
    }
    if (! content.empty())
    {
        if (before)
        {
            dest_page.addPageContents(
                QPDFObjectHandle::newStream(&pdf, content), true);
        }
        else
        {
            dest_page.addPageContents(
                QPDFObjectHandle::newStream(&pdf, "q\n"), true);
            dest_page.addPageContents(
                QPDFObjectHandle::newStream(&pdf, "\nQ\n" + content), false);
        }
    }
}

static void get_uo_pagenos(QPDFJob::UnderOverlay& uo,
                           std::map<int, std::vector<int> >& pagenos)
{
    size_t idx = 0;
    size_t from_size = uo.from_pagenos.size();
    size_t repeat_size = uo.repeat_pagenos.size();
    for (std::vector<int>::iterator iter = uo.to_pagenos.begin();
         iter != uo.to_pagenos.end(); ++iter, ++idx)
    {
        if (idx < from_size)
        {
            pagenos[*iter].push_back(uo.from_pagenos.at(idx));
        }
        else if (repeat_size)
        {
            pagenos[*iter].push_back(
                uo.repeat_pagenos.at((idx - from_size) % repeat_size));
        }
    }
}

void
QPDFJob::handleUnderOverlay(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    validateUnderOverlay(pdf, &o.underlay);
    validateUnderOverlay(pdf, &o.overlay);
    if ((0 == o.underlay.pdf.getPointer()) &&
        (0 == o.overlay.pdf.getPointer()))
    {
        return;
    }
    std::map<int, std::vector<int> > underlay_pagenos;
    get_uo_pagenos(o.underlay, underlay_pagenos);
    std::map<int, std::vector<int> > overlay_pagenos;
    get_uo_pagenos(o.overlay, overlay_pagenos);
    std::map<int, QPDFObjectHandle> underlay_fo;
    std::map<int, QPDFObjectHandle> overlay_fo;
    std::vector<QPDFPageObjectHelper> upages;
    if (o.underlay.pdf.getPointer())
    {
        upages = QPDFPageDocumentHelper(*(o.underlay.pdf)).getAllPages();
    }
    std::vector<QPDFPageObjectHelper> opages;
    if (o.overlay.pdf.getPointer())
    {
        opages = QPDFPageDocumentHelper(*(o.overlay.pdf)).getAllPages();
    }

    QPDFPageDocumentHelper main_pdh(pdf);
    std::vector<QPDFPageObjectHelper> main_pages = main_pdh.getAllPages();
    size_t main_npages = main_pages.size();
    o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": processing underlay/overlay" << std::endl;
    });
    for (size_t i = 0; i < main_npages; ++i)
    {
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << "  page " << 1+i << std::endl;
        });
        doUnderOverlayForPage(pdf, o.underlay, underlay_pagenos, i,
                              underlay_fo, upages, main_pages.at(i),
                              true);
        doUnderOverlayForPage(pdf, o.overlay, overlay_pagenos, i,
                              overlay_fo, opages, main_pages.at(i),
                              false);
    }
}

static void maybe_set_pagemode(QPDF& pdf, std::string const& pagemode)
{
    auto root = pdf.getRoot();
    if (root.getKey("/PageMode").isNull())
    {
        root.replaceKey("/PageMode", QPDFObjectHandle::newName(pagemode));
    }
}

void
QPDFJob::addAttachments(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    maybe_set_pagemode(pdf, "/UseAttachments");
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    std::vector<std::string> duplicated_keys;
    for (auto const& to_add: o.attachments_to_add)
    {
        if ((! to_add.replace) && efdh.getEmbeddedFile(to_add.key))
        {
            duplicated_keys.push_back(to_add.key);
            continue;
        }

        auto fs = QPDFFileSpecObjectHelper::createFileSpec(
            pdf, to_add.filename, to_add.path);
        if (! to_add.description.empty())
        {
            fs.setDescription(to_add.description);
        }
        auto efs = QPDFEFStreamObjectHelper(fs.getEmbeddedFileStream());
        efs.setCreationDate(to_add.creationdate)
            .setModDate(to_add.moddate);
        if (! to_add.mimetype.empty())
        {
            efs.setSubtype(to_add.mimetype);
        }

        efdh.replaceEmbeddedFile(to_add.key, fs);
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": attached " << to_add.path
                 << " as " << to_add.filename
                 << " with key " << to_add.key << std::endl;
        });
    }

    if (! duplicated_keys.empty())
    {
        std::string message;
        for (auto const& k: duplicated_keys)
        {
            if (! message.empty())
            {
                message += ", ";
            }
            message += k;
        }
        message = pdf.getFilename() +
            " already has attachments with the following keys: " +
            message +
            "; use --replace to replace or --key to specify a different key";
        throw std::runtime_error(message);
    }
}

void
QPDFJob::copyAttachments(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    maybe_set_pagemode(pdf, "/UseAttachments");
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    std::vector<std::string> duplicates;
    for (auto const& to_copy: o.attachments_to_copy)
    {
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": copying attachments from "
                 << to_copy.path << std::endl;
        });
        auto other = processFile(
            to_copy.path.c_str(), to_copy.password.c_str());
        QPDFEmbeddedFileDocumentHelper other_efdh(*other);
        auto other_attachments = other_efdh.getEmbeddedFiles();
        for (auto const& iter: other_attachments)
        {
            std::string new_key = to_copy.prefix + iter.first;
            if (efdh.getEmbeddedFile(new_key))
            {
                duplicates.push_back(
                    "file: " + to_copy.path + ", key: " + new_key);
            }
            else
            {
                auto new_fs_oh = pdf.copyForeignObject(
                    iter.second->getObjectHandle());
                efdh.replaceEmbeddedFile(
                    new_key, QPDFFileSpecObjectHelper(new_fs_oh));
                o.doIfVerbose([&](std::ostream& cout,
                                  std::string const& prefix) {
                    cout << "  " << iter.first << " -> " << new_key
                         << std::endl;
                });
            }
        }

        if (other->anyWarnings())
        {
            this->m->warnings = true;
        }
    }

    if (! duplicates.empty())
    {
        std::string message;
        for (auto const& i: duplicates)
        {
            if (! message.empty())
            {
                message += "; ";
            }
            message += i;
        }
        message = pdf.getFilename() +
            " already has attachments with keys that conflict with"
            " attachments from other files: " + message +
            ". Use --prefix with --copy-attachments-from"
            " or manually copy individual attachments.";
        throw std::runtime_error(message);
    }
}

void
QPDFJob::handleTransformations(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    QPDFPageDocumentHelper dh(pdf);
    PointerHolder<QPDFAcroFormDocumentHelper> afdh;
    auto make_afdh = [&]() {
        if (! afdh.getPointer())
        {
            afdh = new QPDFAcroFormDocumentHelper(pdf);
        }
    };
    if (o.externalize_inline_images)
    {
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
             iter != pages.end(); ++iter)
        {
            QPDFPageObjectHelper& ph(*iter);
            ph.externalizeInlineImages(o.ii_min_bytes);
        }
    }
    if (o.optimize_images)
    {
        int pageno = 0;
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
             iter != pages.end(); ++iter)
        {
            ++pageno;
            QPDFPageObjectHelper& ph(*iter);
            QPDFObjectHandle page = ph.getObjectHandle();
            std::map<std::string, QPDFObjectHandle> images = ph.getImages();
            for (auto& iter2: images)
            {
                std::string name = iter2.first;
                QPDFObjectHandle& image = iter2.second;
                ImageOptimizer* io = new ImageOptimizer(o, image);
                PointerHolder<QPDFObjectHandle::StreamDataProvider> sdp(io);
                if (io->evaluate("image " + name + " on page " +
                                 QUtil::int_to_string(pageno)))
                {
                    QPDFObjectHandle new_image =
                        QPDFObjectHandle::newStream(&pdf);
                    new_image.replaceDict(image.getDict().shallowCopy());
                    new_image.replaceStreamData(
                        sdp,
                        QPDFObjectHandle::newName("/DCTDecode"),
                        QPDFObjectHandle::newNull());
                    ph.getAttribute("/Resources", true).
                        getKey("/XObject").replaceKey(
                            name, new_image);
                }
            }
        }
    }
    if (o.generate_appearances)
    {
        make_afdh();
        afdh->generateAppearancesIfNeeded();
    }
    if (o.flatten_annotations)
    {
        dh.flattenAnnotations(o.flatten_annotations_required,
                              o.flatten_annotations_forbidden);
    }
    if (o.coalesce_contents)
    {
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
             iter != pages.end(); ++iter)
        {
            (*iter).coalesceContentStreams();
        }
    }
    if (o.flatten_rotation)
    {
        make_afdh();
        for (auto& page: dh.getAllPages())
        {
            page.flattenRotation(afdh.getPointer());
        }
    }
    if (o.remove_page_labels)
    {
        pdf.getRoot().removeKey("/PageLabels");
    }
    if (! o.attachments_to_remove.empty())
    {
        QPDFEmbeddedFileDocumentHelper efdh(pdf);
        for (auto const& key: o.attachments_to_remove)
        {
            if (efdh.removeEmbeddedFile(key))
            {
                o.doIfVerbose([&](std::ostream& cout,
                                  std::string const& prefix) {
                    cout << prefix <<
                        ": removed attachment " << key << std::endl;
                });
            }
            else
            {
                throw std::runtime_error("attachment " + key + " not found");
            }
        }
    }
    if (! o.attachments_to_add.empty())
    {
        addAttachments(pdf);
    }
    if (! o.attachments_to_copy.empty())
    {
        copyAttachments(pdf);
    }
}

bool
QPDFJob::shouldRemoveUnreferencedResources(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    if (o.remove_unreferenced_page_resources == QPDFJob::re_no)
    {
        return false;
    }
    else if (o.remove_unreferenced_page_resources == QPDFJob::re_yes)
    {
        return true;
    }

    // Unreferenced resources are common in files where resources
    // dictionaries are shared across pages. As a heuristic, we look
    // in the file for shared resources dictionaries or shared XObject
    // subkeys of resources dictionaries either on pages or on form
    // XObjects in pages. If we find any, then there is a higher
    // likelihood that the expensive process of finding unreferenced
    // resources is worth it.

    // Return true as soon as we find any shared resources.

    std::set<QPDFObjGen> resources_seen;      // shared resources detection
    std::set<QPDFObjGen> nodes_seen;          // loop detection

    o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": " << pdf.getFilename()
             << ": checking for shared resources" << std::endl;
    });

    std::list<QPDFObjectHandle> queue;
    queue.push_back(pdf.getRoot().getKey("/Pages"));
    while (! queue.empty())
    {
        QPDFObjectHandle node = *queue.begin();
        queue.pop_front();
        QPDFObjGen og = node.getObjGen();
        if (nodes_seen.count(og))
        {
            continue;
        }
        nodes_seen.insert(og);
        QPDFObjectHandle dict = node.isStream() ? node.getDict() : node;
        QPDFObjectHandle kids = dict.getKey("/Kids");
        if (kids.isArray())
        {
            // This is a non-leaf node.
            if (dict.hasKey("/Resources"))
            {
                QTC::TC("qpdf", "qpdf found resources in non-leaf");
                o.doIfVerbose([&](std::ostream& cout,
                                  std::string const& prefix) {
                    cout << "  found resources in non-leaf page node "
                         << og.getObj() << " " << og.getGen()
                         << std::endl;
                });
                return true;
            }
            int n = kids.getArrayNItems();
            for (int i = 0; i < n; ++i)
            {
                queue.push_back(kids.getArrayItem(i));
            }
        }
        else
        {
            // This is a leaf node or a form XObject.
            QPDFObjectHandle resources = dict.getKey("/Resources");
            if (resources.isIndirect())
            {
                QPDFObjGen resources_og = resources.getObjGen();
                if (resources_seen.count(resources_og))
                {
                    QTC::TC("qpdf", "qpdf found shared resources in leaf");
                    o.doIfVerbose([&](std::ostream& cout,
                                      std::string const& prefix) {
                        cout << "  found shared resources in leaf node "
                             << og.getObj() << " " << og.getGen()
                             << ": "
                             << resources_og.getObj() << " "
                             << resources_og.getGen()
                             << std::endl;
                    });
                    return true;
                }
                resources_seen.insert(resources_og);
            }
            QPDFObjectHandle xobject = (resources.isDictionary() ?
                                        resources.getKey("/XObject") :
                                        QPDFObjectHandle::newNull());
            if (xobject.isIndirect())
            {
                QPDFObjGen xobject_og = xobject.getObjGen();
                if (resources_seen.count(xobject_og))
                {
                    QTC::TC("qpdf", "qpdf found shared xobject in leaf");
                    o.doIfVerbose([&](std::ostream& cout,
                                      std::string const& prefix) {
                        cout << "  found shared xobject in leaf node "
                             << og.getObj() << " " << og.getGen()
                             << ": "
                             << xobject_og.getObj() << " "
                             << xobject_og.getGen()
                             << std::endl;
                    });
                    return true;
                }
                resources_seen.insert(xobject_og);
            }
            if (xobject.isDictionary())
            {
                for (auto const& k: xobject.getKeys())
                {
                    QPDFObjectHandle xobj = xobject.getKey(k);
                    if (xobj.isStream() &&
                        xobj.getDict().getKey("/Type").isName() &&
                        ("/XObject" ==
                         xobj.getDict().getKey("/Type").getName()) &&
                        xobj.getDict().getKey("/Subtype").isName() &&
                        ("/Form" ==
                         xobj.getDict().getKey("/Subtype").getName()))
                    {
                        queue.push_back(xobj);
                    }
                }
            }
        }
    }

    o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": no shared resources found" << std::endl;
    });
    return false;
}

static QPDFObjectHandle added_page(QPDF& pdf, QPDFObjectHandle page)
{
    QPDFObjectHandle result = page;
    if (page.getOwningQPDF() != &pdf)
    {
        // Calling copyForeignObject on an object we already copied
        // will give us the already existing copy.
        result = pdf.copyForeignObject(page);
    }
    return result;
}

static QPDFObjectHandle added_page(QPDF& pdf, QPDFPageObjectHelper page)
{
    return added_page(pdf, page.getObjectHandle());
}

void
QPDFJob::handlePageSpecs(
    QPDF& pdf, bool& warnings,
    std::vector<PointerHolder<QPDF>>& page_heap)
{
    QPDFJob& o = *this; // QXXXQ
    // Parse all page specifications and translate them into lists of
    // actual pages.

    // Handle "." as a shortcut for the input file
    for (std::vector<QPDFJob::PageSpec>::iterator iter = o.page_specs.begin();
         iter != o.page_specs.end(); ++iter)
    {
        QPDFJob::PageSpec& page_spec = *iter;
        if (page_spec.filename == ".")
        {
            page_spec.filename = o.infilename;
        }
    }

    if (! o.keep_files_open_set)
    {
        // Count the number of distinct files to determine whether we
        // should keep files open or not. Rather than trying to code
        // some portable heuristic based on OS limits, just hard-code
        // this at a given number and allow users to override.
        std::set<std::string> filenames;
        for (auto& page_spec: o.page_specs)
        {
            filenames.insert(page_spec.filename);
        }
        if (filenames.size() > o.keep_files_open_threshold)
        {
            QTC::TC("qpdf", "qpdf disable keep files open");
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": selecting --keep-open-files=n"
                     << std::endl;
            });
            o.keep_files_open = false;
        }
        else
        {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": selecting --keep-open-files=y"
                     << std::endl;
            });
            o.keep_files_open = true;
            QTC::TC("qpdf", "qpdf don't disable keep files open");
        }
    }

    // Create a QPDF object for each file that we may take pages from.
    std::map<std::string, QPDF*> page_spec_qpdfs;
    std::map<std::string, ClosedFileInputSource*> page_spec_cfis;
    page_spec_qpdfs[o.infilename] = &pdf;
    std::vector<QPDFPageData> parsed_specs;
    std::map<unsigned long long, std::set<QPDFObjGen> > copied_pages;
    for (std::vector<QPDFJob::PageSpec>::iterator iter = o.page_specs.begin();
         iter != o.page_specs.end(); ++iter)
    {
        QPDFJob::PageSpec& page_spec = *iter;
        if (page_spec_qpdfs.count(page_spec.filename) == 0)
        {
            // Open the PDF file and store the QPDF object. Throw a
            // PointerHolder to the qpdf into a heap so that it
            // survives through copying to the output but gets cleaned up
            // automatically at the end. Do not canonicalize the file
            // name. Using two different paths to refer to the same
            // file is a document workaround for duplicating a page.
            // If you are using this an example of how to do this with
            // the API, you can just create two different QPDF objects
            // to the same underlying file with the same path to
            // achieve the same affect.
            char const* password = page_spec.password;
            if (o.encryption_file && (password == 0) &&
                (page_spec.filename == o.encryption_file))
            {
                QTC::TC("qpdf", "qpdf pages encryption password");
                password = o.encryption_file_password;
            }
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": processing "
                     << page_spec.filename << std::endl;
            });
            PointerHolder<InputSource> is;
            ClosedFileInputSource* cis = 0;
            if (! o.keep_files_open)
            {
                QTC::TC("qpdf", "qpdf keep files open n");
                cis = new ClosedFileInputSource(page_spec.filename.c_str());
                is = cis;
                cis->stayOpen(true);
            }
            else
            {
                QTC::TC("qpdf", "qpdf keep files open y");
                FileInputSource* fis = new FileInputSource();
                is = fis;
                fis->setFilename(page_spec.filename.c_str());
            }
            PointerHolder<QPDF> qpdf_ph = processInputSource(is, password);
            page_heap.push_back(qpdf_ph);
            page_spec_qpdfs[page_spec.filename] = qpdf_ph.getPointer();
            if (cis)
            {
                cis->stayOpen(false);
                page_spec_cfis[page_spec.filename] = cis;
            }
        }

        // Read original pages from the PDF, and parse the page range
        // associated with this occurrence of the file.
        parsed_specs.push_back(
            QPDFPageData(page_spec.filename,
                         page_spec_qpdfs[page_spec.filename],
                         page_spec.range));
    }

    std::map<unsigned long long, bool> remove_unreferenced;
    if (o.remove_unreferenced_page_resources != QPDFJob::re_no)
    {
        for (std::map<std::string, QPDF*>::iterator iter =
                 page_spec_qpdfs.begin();
             iter != page_spec_qpdfs.end(); ++iter)
        {
            std::string const& filename = (*iter).first;
            ClosedFileInputSource* cis = 0;
            if (page_spec_cfis.count(filename))
            {
                cis = page_spec_cfis[filename];
                cis->stayOpen(true);
            }
            QPDF& other(*((*iter).second));
            auto other_uuid = other.getUniqueId();
            if (remove_unreferenced.count(other_uuid) == 0)
            {
                remove_unreferenced[other_uuid] =
                    shouldRemoveUnreferencedResources(other);
            }
            if (cis)
            {
                cis->stayOpen(false);
            }
        }
    }

    // Clear all pages out of the primary QPDF's pages tree but leave
    // the objects in place in the file so they can be re-added
    // without changing their object numbers. This enables other
    // things in the original file, such as outlines, to continue to
    // work.
    o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix
             << ": removing unreferenced pages from primary input"
             << std::endl;
    });
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> orig_pages = dh.getAllPages();
    for (std::vector<QPDFPageObjectHelper>::iterator iter =
             orig_pages.begin();
         iter != orig_pages.end(); ++iter)
    {
        dh.removePage(*iter);
    }

    if (o.collate && (parsed_specs.size() > 1))
    {
        // Collate the pages by selecting one page from each spec in
        // order. When a spec runs out of pages, stop selecting from
        // it.
        std::vector<QPDFPageData> new_parsed_specs;
        size_t nspecs = parsed_specs.size();
        size_t cur_page = 0;
        bool got_pages = true;
        while (got_pages)
        {
            got_pages = false;
            for (size_t i = 0; i < nspecs; ++i)
            {
                QPDFPageData& page_data = parsed_specs.at(i);
                for (size_t j = 0; j < o.collate; ++j)
                {
                    if (cur_page + j < page_data.selected_pages.size())
                    {
                        got_pages = true;
                        new_parsed_specs.push_back(
                            QPDFPageData(
                                page_data,
                                page_data.selected_pages.at(cur_page + j)));
                    }
                }
            }
            cur_page += o.collate;
        }
        parsed_specs = new_parsed_specs;
    }

    // Add all the pages from all the files in the order specified.
    // Keep track of any pages from the original file that we are
    // selecting.
    std::set<int> selected_from_orig;
    std::vector<QPDFObjectHandle> new_labels;
    bool any_page_labels = false;
    int out_pageno = 0;
    std::map<unsigned long long,
             PointerHolder<QPDFAcroFormDocumentHelper>> afdh_map;
    auto this_afdh = get_afdh_for_qpdf(afdh_map, &pdf);
    std::set<QPDFObjGen> referenced_fields;
    for (std::vector<QPDFPageData>::iterator iter =
             parsed_specs.begin();
         iter != parsed_specs.end(); ++iter)
    {
        QPDFPageData& page_data = *iter;
        ClosedFileInputSource* cis = 0;
        if (page_spec_cfis.count(page_data.filename))
        {
            cis = page_spec_cfis[page_data.filename];
            cis->stayOpen(true);
        }
        QPDFPageLabelDocumentHelper pldh(*page_data.qpdf);
        auto other_afdh = get_afdh_for_qpdf(afdh_map, page_data.qpdf);
        if (pldh.hasPageLabels())
        {
            any_page_labels = true;
        }
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": adding pages from "
                 << page_data.filename << std::endl;
        });
        for (std::vector<int>::iterator pageno_iter =
                 page_data.selected_pages.begin();
             pageno_iter != page_data.selected_pages.end();
             ++pageno_iter, ++out_pageno)
        {
            // Pages are specified from 1 but numbered from 0 in the
            // vector
            int pageno = *pageno_iter - 1;
            pldh.getLabelsForPageRange(pageno, pageno, out_pageno,
                                       new_labels);
            QPDFPageObjectHelper to_copy =
                page_data.orig_pages.at(QIntC::to_size(pageno));
            QPDFObjGen to_copy_og = to_copy.getObjectHandle().getObjGen();
            unsigned long long from_uuid = page_data.qpdf->getUniqueId();
            if (copied_pages[from_uuid].count(to_copy_og))
            {
                QTC::TC("qpdf", "qpdf copy same page more than once",
                        (page_data.qpdf == &pdf) ? 0 : 1);
                to_copy = to_copy.shallowCopyPage();
            }
            else
            {
                copied_pages[from_uuid].insert(to_copy_og);
                if (remove_unreferenced[from_uuid])
                {
                    to_copy.removeUnreferencedResources();
                }
            }
            dh.addPage(to_copy, false);
            bool first_copy_from_orig = false;
            bool this_file = (page_data.qpdf == &pdf);
            if (this_file)
            {
                // This is a page from the original file. Keep track
                // of the fact that we are using it.
                first_copy_from_orig = (selected_from_orig.count(pageno) == 0);
                selected_from_orig.insert(pageno);
            }
            auto new_page = added_page(pdf, to_copy);
            // Try to avoid gratuitously renaming fields. In the case
            // of where we're just extracting a bunch of pages from
            // the original file and not copying any page more than
            // once, there's no reason to do anything with the fields.
            // Since we don't remove fields from the original file
            // until all copy operations are completed, any foreign
            // pages that conflict with original pages will be
            // adjusted. If we copy any page from the original file
            // more than once, that page would be in conflict with the
            // previous copy of itself.
            if (other_afdh->hasAcroForm() &&
                ((! this_file) || (! first_copy_from_orig)))
            {
                if (! this_file)
                {
                    QTC::TC("qpdf", "qpdf copy fields not this file");
                }
                else if (! first_copy_from_orig)
                {
                    QTC::TC("qpdf", "qpdf copy fields non-first from orig");
                }
                try
                {
                    this_afdh->fixCopiedAnnotations(
                        new_page, to_copy.getObjectHandle(), *other_afdh,
                        &referenced_fields);
                }
                catch (std::exception& e)
                {
                    pdf.warn(
                        QPDFExc(qpdf_e_damaged_pdf, pdf.getFilename(),
                                "", 0, "Exception caught while fixing copied"
                                " annotations. This may be a qpdf bug. " +
                                std::string("Exception: ") + e.what()));
                }
            }
        }
        if (page_data.qpdf->anyWarnings())
        {
            warnings = true;
        }
        if (cis)
        {
            cis->stayOpen(false);
        }
    }
    if (any_page_labels)
    {
        QPDFObjectHandle page_labels =
            QPDFObjectHandle::newDictionary();
        page_labels.replaceKey(
            "/Nums", QPDFObjectHandle::newArray(new_labels));
        pdf.getRoot().replaceKey("/PageLabels", page_labels);
    }

    // Delete page objects for unused page in primary. This prevents
    // those objects from being preserved by being referred to from
    // other places, such as the outlines dictionary. Also make sure
    // we keep form fields from pages we preserved.
    for (size_t pageno = 0; pageno < orig_pages.size(); ++pageno)
    {
        auto page = orig_pages.at(pageno);
        if (selected_from_orig.count(QIntC::to_int(pageno)))
        {
            for (auto field: this_afdh->getFormFieldsForPage(page))
            {
                QTC::TC("qpdf", "qpdf pages keeping field from original");
                referenced_fields.insert(field.getObjectHandle().getObjGen());
            }
        }
        else
        {
            pdf.replaceObject(
                page.getObjectHandle().getObjGen(),
                QPDFObjectHandle::newNull());
        }
    }
    // Remove unreferenced form fields
    if (this_afdh->hasAcroForm())
    {
        auto acroform = pdf.getRoot().getKey("/AcroForm");
        auto fields = acroform.getKey("/Fields");
        if (fields.isArray())
        {
            auto new_fields = QPDFObjectHandle::newArray();
            if (fields.isIndirect())
            {
                new_fields = pdf.makeIndirectObject(new_fields);
            }
            for (auto const& field: fields.aitems())
            {
                if (referenced_fields.count(field.getObjGen()))
                {
                    new_fields.appendItem(field);
                }
            }
            if (new_fields.getArrayNItems() > 0)
            {
                QTC::TC("qpdf", "qpdf keep some fields in pages");
                acroform.replaceKey("/Fields", new_fields);
            }
            else
            {
                QTC::TC("qpdf", "qpdf no more fields in pages");
                pdf.getRoot().removeKey("/AcroForm");
            }
        }
    }
}

void
QPDFJob::handleRotations(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    int npages = QIntC::to_int(pages.size());
    for (std::map<std::string, QPDFJob::RotationSpec>::iterator iter =
             o.rotations.begin();
         iter != o.rotations.end(); ++iter)
    {
        std::string const& range = (*iter).first;
        QPDFJob::RotationSpec const& rspec = (*iter).second;
        // range has been previously validated
        std::vector<int> to_rotate =
            QUtil::parse_numrange(range.c_str(), npages);
        for (std::vector<int>::iterator i2 = to_rotate.begin();
             i2 != to_rotate.end(); ++i2)
        {
            int pageno = *i2 - 1;
            if ((pageno >= 0) && (pageno < npages))
            {
                pages.at(QIntC::to_size(pageno)).rotatePage(
                    rspec.angle, rspec.relative);
            }
        }
    }
}

void
QPDFJob::maybeFixWritePassword(int R, std::string& password)
{
    QPDFJob& o = *this; // QXXXQ
    switch (o.password_mode)
    {
      case QPDFJob::pm_bytes:
        QTC::TC("qpdf", "qpdf password mode bytes");
        break;

      case QPDFJob::pm_hex_bytes:
        QTC::TC("qpdf", "qpdf password mode hex-bytes");
        password = QUtil::hex_decode(password);
        break;

      case QPDFJob::pm_unicode:
      case QPDFJob::pm_auto:
        {
            bool has_8bit_chars;
            bool is_valid_utf8;
            bool is_utf16;
            QUtil::analyze_encoding(password,
                                    has_8bit_chars,
                                    is_valid_utf8,
                                    is_utf16);
            if (! has_8bit_chars)
            {
                return;
            }
            if (o.password_mode == QPDFJob::pm_unicode)
            {
                if (! is_valid_utf8)
                {
                    QTC::TC("qpdf", "qpdf password not unicode");
                    throw std::runtime_error(
                        "supplied password is not valid UTF-8");
                }
                if (R < 5)
                {
                    std::string encoded;
                    if (! QUtil::utf8_to_pdf_doc(password, encoded))
                    {
                        QTC::TC("qpdf", "qpdf password not encodable");
                        throw std::runtime_error(
                            "supplied password cannot be encoded for"
                            " 40-bit or 128-bit encryption formats");
                    }
                    password = encoded;
                }
            }
            else
            {
                if ((R < 5) && is_valid_utf8)
                {
                    std::string encoded;
                    if (QUtil::utf8_to_pdf_doc(password, encoded))
                    {
                        QTC::TC("qpdf", "qpdf auto-encode password");
                        o.doIfVerbose([&](std::ostream& cout,
                                          std::string const& prefix) {
                            cout
                                << prefix
                                << ": automatically converting Unicode"
                                << " password to single-byte encoding as"
                                << " required for 40-bit or 128-bit"
                                << " encryption" << std::endl;
                        });
                        password = encoded;
                    }
                    else
                    {
                        QTC::TC("qpdf", "qpdf bytes fallback warning");
                        *(this->m->cerr)
                            << this->m->message_prefix << ": WARNING: "
                            << "supplied password looks like a Unicode"
                            << " password with characters not allowed in"
                            << " passwords for 40-bit and 128-bit encryption;"
                            << " most readers will not be able to open this"
                            << " file with the supplied password."
                            << " (Use --password-mode=bytes to suppress this"
                            << " warning and use the password anyway.)"
                            << std::endl;
                    }
                }
                else if ((R >= 5) && (! is_valid_utf8))
                {
                    QTC::TC("qpdf", "qpdf invalid utf-8 in auto");
                    throw std::runtime_error(
                        "supplied password is not a valid Unicode password,"
                        " which is required for 256-bit encryption; to"
                        " really use this password, rerun with the"
                        " --password-mode=bytes option");
                }
            }
        }
        break;
    }
}

void
QPDFJob::setEncryptionOptions(QPDF& pdf, QPDFWriter& w)
{
    QPDFJob& o = *this; // QXXXQ
    int R = 0;
    if (o.keylen == 40)
    {
        R = 2;
    }
    else if (o.keylen == 128)
    {
        if (o.force_V4 || o.cleartext_metadata || o.use_aes)
        {
            R = 4;
        }
        else
        {
            R = 3;
        }
    }
    else if (o.keylen == 256)
    {
        if (o.force_R5)
        {
            R = 5;
        }
        else
        {
            R = 6;
        }
    }
    else
    {
        throw std::logic_error("bad encryption keylen");
    }
    if ((R > 3) && (o.r3_accessibility == false))
    {
        *(this->m->cerr)
            << this->m->message_prefix
            << ": -accessibility=n is ignored for modern"
            << " encryption formats" << std::endl;
    }
    maybeFixWritePassword(R, o.user_password);
    maybeFixWritePassword(R, o.owner_password);
    if ((R < 4) || ((R == 4) && (! o.use_aes)))
    {
        if (! o.allow_weak_crypto)
        {
            // Do not set warnings = true for this case as this does
            // not reflect a potential problem with the input file.
            QTC::TC("qpdf", "qpdf weak crypto warning");
            *(this->m->cerr)
                << this->m->message_prefix
                << ": writing a file with RC4, a weak cryptographic algorithm"
                << std::endl
                << "Please use 256-bit keys for better security."
                << std::endl
                << "Pass --allow-weak-crypto to suppress this warning."
                << std::endl
                << "This will become an error in a future version of qpdf."
                << std::endl;
        }
    }
    switch (R)
    {
      case 2:
        w.setR2EncryptionParameters(
            o.user_password.c_str(), o.owner_password.c_str(),
            o.r2_print, o.r2_modify, o.r2_extract, o.r2_annotate);
        break;
      case 3:
        w.setR3EncryptionParameters(
            o.user_password.c_str(), o.owner_password.c_str(),
            o.r3_accessibility, o.r3_extract,
            o.r3_assemble, o.r3_annotate_and_form,
            o.r3_form_filling, o.r3_modify_other,
            o.r3_print);
        break;
      case 4:
        w.setR4EncryptionParameters(
            o.user_password.c_str(), o.owner_password.c_str(),
            o.r3_accessibility, o.r3_extract,
            o.r3_assemble, o.r3_annotate_and_form,
            o.r3_form_filling, o.r3_modify_other,
            o.r3_print, !o.cleartext_metadata, o.use_aes);
        break;
      case 5:
        w.setR5EncryptionParameters(
            o.user_password.c_str(), o.owner_password.c_str(),
            o.r3_accessibility, o.r3_extract,
            o.r3_assemble, o.r3_annotate_and_form,
            o.r3_form_filling, o.r3_modify_other,
            o.r3_print, !o.cleartext_metadata);
        break;
      case 6:
        w.setR6EncryptionParameters(
            o.user_password.c_str(), o.owner_password.c_str(),
            o.r3_accessibility, o.r3_extract,
            o.r3_assemble, o.r3_annotate_and_form,
            o.r3_form_filling, o.r3_modify_other,
            o.r3_print, !o.cleartext_metadata);
        break;
      default:
        throw std::logic_error("bad encryption R value");
        break;
    }
}

static void parse_version(std::string const& full_version_string,
                          std::string& version, int& extension_level)
{
    PointerHolder<char> vp(true, QUtil::copy_string(full_version_string));
    char* v = vp.getPointer();
    char* p1 = strchr(v, '.');
    char* p2 = (p1 ? strchr(1 + p1, '.') : 0);
    if (p2 && *(p2 + 1))
    {
        *p2++ = '\0';
        extension_level = QUtil::string_to_int(p2);
    }
    version = v;
}

void
QPDFJob::setWriterOptions(QPDF& pdf, QPDFWriter& w)
{
    QPDFJob& o = *this; // QXXXQ
    if (o.compression_level >= 0)
    {
        Pl_Flate::setCompressionLevel(o.compression_level);
    }
    if (o.qdf_mode)
    {
        w.setQDFMode(true);
    }
    if (o.preserve_unreferenced_objects)
    {
        w.setPreserveUnreferencedObjects(true);
    }
    if (o.newline_before_endstream)
    {
        w.setNewlineBeforeEndstream(true);
    }
    if (o.normalize_set)
    {
        w.setContentNormalization(o.normalize);
    }
    if (o.stream_data_set)
    {
        w.setStreamDataMode(o.stream_data_mode);
    }
    if (o.compress_streams_set)
    {
        w.setCompressStreams(o.compress_streams);
    }
    if (o.recompress_flate_set)
    {
        w.setRecompressFlate(o.recompress_flate);
    }
    if (o.decode_level_set)
    {
        w.setDecodeLevel(o.decode_level);
    }
    if (o.decrypt)
    {
        w.setPreserveEncryption(false);
    }
    if (o.deterministic_id)
    {
        w.setDeterministicID(true);
    }
    if (o.static_id)
    {
        w.setStaticID(true);
    }
    if (o.static_aes_iv)
    {
        w.setStaticAesIV(true);
    }
    if (o.suppress_original_object_id)
    {
        w.setSuppressOriginalObjectIDs(true);
    }
    if (o.copy_encryption)
    {
        PointerHolder<QPDF> encryption_pdf =
            processFile(o.encryption_file, o.encryption_file_password);
        w.copyEncryptionParameters(*encryption_pdf);
    }
    if (o.encrypt)
    {
        setEncryptionOptions(pdf, w);
    }
    if (o.linearize)
    {
        w.setLinearization(true);
    }
    if (! o.linearize_pass1.empty())
    {
        w.setLinearizationPass1Filename(o.linearize_pass1);
    }
    if (o.object_stream_set)
    {
        w.setObjectStreamMode(o.object_stream_mode);
    }
    if (! o.min_version.empty())
    {
        std::string version;
        int extension_level = 0;
        parse_version(o.min_version, version, extension_level);
        w.setMinimumPDFVersion(version, extension_level);
    }
    if (! o.force_version.empty())
    {
        std::string version;
        int extension_level = 0;
        parse_version(o.force_version, version, extension_level);
        w.forcePDFVersion(version, extension_level);
    }
    if (o.progress && o.outfilename)
    {
        w.registerProgressReporter(
            new ProgressReporter(
                *(this->m->cout), this->m->message_prefix, o.outfilename));
    }
}

void
QPDFJob::doSplitPages(QPDF& pdf, bool& warnings)
{
    QPDFJob& o = *this; // QXXXQ
    // Generate output file pattern
    std::string before;
    std::string after;
    size_t len = strlen(o.outfilename);
    char* num_spot = strstr(const_cast<char*>(o.outfilename), "%d");
    if (num_spot != 0)
    {
        QTC::TC("qpdf", "qpdf split-pages %d");
        before = std::string(o.outfilename,
                             QIntC::to_size(num_spot - o.outfilename));
        after = num_spot + 2;
    }
    else if ((len >= 4) &&
             (QUtil::str_compare_nocase(
                 o.outfilename + len - 4, ".pdf") == 0))
    {
        QTC::TC("qpdf", "qpdf split-pages .pdf");
        before = std::string(o.outfilename, len - 4) + "-";
        after = o.outfilename + len - 4;
    }
    else
    {
        QTC::TC("qpdf", "qpdf split-pages other");
        before = std::string(o.outfilename) + "-";
    }

    if (shouldRemoveUnreferencedResources(pdf))
    {
        QPDFPageDocumentHelper dh(pdf);
        dh.removeUnreferencedResources();
    }
    QPDFPageLabelDocumentHelper pldh(pdf);
    QPDFAcroFormDocumentHelper afdh(pdf);
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
    size_t pageno_len = QUtil::uint_to_string(pages.size()).length();
    size_t num_pages = pages.size();
    for (size_t i = 0; i < num_pages; i += QIntC::to_size(o.split_pages))
    {
        size_t first = i + 1;
        size_t last = i + QIntC::to_size(o.split_pages);
        if (last > num_pages)
        {
            last = num_pages;
        }
        QPDF outpdf;
        outpdf.emptyPDF();
        PointerHolder<QPDFAcroFormDocumentHelper> out_afdh;
        if (afdh.hasAcroForm())
        {
            out_afdh = new QPDFAcroFormDocumentHelper(outpdf);
        }
        if (o.suppress_warnings)
        {
            outpdf.setSuppressWarnings(true);
        }
        for (size_t pageno = first; pageno <= last; ++pageno)
        {
            QPDFObjectHandle page = pages.at(pageno - 1);
            outpdf.addPage(page, false);
            auto new_page = added_page(outpdf, page);
            if (out_afdh.getPointer())
            {
                QTC::TC("qpdf", "qpdf copy form fields in split_pages");
                try
                {
                    out_afdh->fixCopiedAnnotations(new_page, page, afdh);
                }
                catch (std::exception& e)
                {
                    pdf.warn(
                        QPDFExc(qpdf_e_damaged_pdf, pdf.getFilename(),
                                "", 0, "Exception caught while fixing copied"
                                " annotations. This may be a qpdf bug." +
                                std::string("Exception: ") + e.what()));
                }
            }
        }
        if (pldh.hasPageLabels())
        {
            std::vector<QPDFObjectHandle> labels;
            pldh.getLabelsForPageRange(
                QIntC::to_longlong(first - 1),
                QIntC::to_longlong(last - 1),
                0, labels);
            QPDFObjectHandle page_labels =
                QPDFObjectHandle::newDictionary();
            page_labels.replaceKey(
                "/Nums", QPDFObjectHandle::newArray(labels));
            outpdf.getRoot().replaceKey("/PageLabels", page_labels);
        }
        std::string page_range =
            QUtil::uint_to_string(first, QIntC::to_int(pageno_len));
        if (o.split_pages > 1)
        {
            page_range += "-" +
                QUtil::uint_to_string(last, QIntC::to_int(pageno_len));
        }
        std::string outfile = before + page_range + after;
        if (QUtil::same_file(o.infilename, outfile.c_str()))
        {
            throw std::runtime_error(
                "split pages would overwrite input file with " + outfile);
        }
        QPDFWriter w(outpdf, outfile.c_str());
        setWriterOptions(outpdf, w);
        w.write();
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": wrote file " << outfile << std::endl;
        });
        if (outpdf.anyWarnings())
        {
            warnings = true;
        }
    }
}

void
QPDFJob::writeOutfile(QPDF& pdf)
{
    QPDFJob& o = *this; // QXXXQ
    std::string temp_out;
    if (o.replace_input)
    {
        // Append but don't prepend to the path to generate a
        // temporary name. This saves us from having to split the path
        // by directory and non-directory.
        temp_out = std::string(o.infilename) + ".~qpdf-temp#";
        // o.outfilename will be restored to 0 before temp_out
        // goes out of scope.
        o.outfilename = temp_out.c_str();
    }
    else if (strcmp(o.outfilename, "-") == 0)
    {
        o.outfilename = 0;
    }
    {
        // Private scope so QPDFWriter will close the output file
        QPDFWriter w(pdf, o.outfilename);
        setWriterOptions(pdf, w);
        w.write();
    }
    if (o.outfilename)
    {
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": wrote file " << o.outfilename << std::endl;
        });
    }
    if (o.replace_input)
    {
        o.outfilename = 0;
    }
    if (o.replace_input)
    {
        // We must close the input before we can rename files
        pdf.closeInputSource();
        std::string backup = std::string(o.infilename) + ".~qpdf-orig";
        bool warnings = pdf.anyWarnings();
        if (! warnings)
        {
            backup.append(1, '#');
        }
        QUtil::rename_file(o.infilename, backup.c_str());
        QUtil::rename_file(temp_out.c_str(), o.infilename);
        if (warnings)
        {
            *(this->m->cerr)
                << this->m->message_prefix
                << ": there are warnings; original file kept in "
                << backup << std::endl;
        }
        else
        {
            try
            {
                QUtil::remove_file(backup.c_str());
            }
            catch (QPDFSystemError& e)
            {
                *(this->m->cerr)
                    << this->m->message_prefix
                    << ": unable to delete original file ("
                    << e.what() << ");"
                    << " original file left in " << backup
                    << ", but the input was successfully replaced"
                    << std::endl;
            }
        }
    }
}

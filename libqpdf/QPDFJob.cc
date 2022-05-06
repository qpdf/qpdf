#include <qpdf/QPDFJob.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <fcntl.h>
#include <iostream>
#include <memory>

#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_OStream.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QPDFWriter.hh>

#include <qpdf/auto_job_schema.hh> // JOB_SCHEMA_DATA

namespace
{
    class ImageOptimizer: public QPDFObjectHandle::StreamDataProvider
    {
      public:
        ImageOptimizer(
            QPDFJob& o,
            size_t oi_min_width,
            size_t oi_min_height,
            size_t oi_min_area,
            QPDFObjectHandle& image);
        virtual ~ImageOptimizer() = default;
        virtual void
        provideStreamData(int objid, int generation, Pipeline* pipeline);
        std::shared_ptr<Pipeline>
        makePipeline(std::string const& description, Pipeline* next);
        bool evaluate(std::string const& description);

      private:
        QPDFJob& o;
        size_t oi_min_width;
        size_t oi_min_height;
        size_t oi_min_area;
        QPDFObjectHandle image;
    };

    class DiscardContents: public QPDFObjectHandle::ParserCallbacks
    {
      public:
        virtual ~DiscardContents() = default;
        virtual void
        handleObject(QPDFObjectHandle)
        {
        }
        virtual void
        handleEOF()
        {
        }
    };

    struct QPDFPageData
    {
        QPDFPageData(
            std::string const& filename, QPDF* qpdf, std::string const& range);
        QPDFPageData(QPDFPageData const& other, int page);

        std::string filename;
        QPDF* qpdf;
        std::vector<QPDFObjectHandle> orig_pages;
        std::vector<int> selected_pages;
    };

    class ProgressReporter: public QPDFWriter::ProgressReporter
    {
      public:
        ProgressReporter(
            std::ostream& cout,
            std::string const& prefix,
            char const* filename) :
            cout(cout),
            prefix(prefix),
            filename(filename)
        {
        }
        virtual ~ProgressReporter() = default;
        virtual void reportProgress(int);

      private:
        std::ostream& cout;
        std::string prefix;
        std::string filename;
    };
} // namespace

ImageOptimizer::ImageOptimizer(
    QPDFJob& o,
    size_t oi_min_width,
    size_t oi_min_height,
    size_t oi_min_area,
    QPDFObjectHandle& image) :
    o(o),
    oi_min_width(oi_min_width),
    oi_min_height(oi_min_height),
    oi_min_area(oi_min_area),
    image(image)
{
}

std::shared_ptr<Pipeline>
ImageOptimizer::makePipeline(std::string const& description, Pipeline* next)
{
    std::shared_ptr<Pipeline> result;
    QPDFObjectHandle dict = image.getDict();
    QPDFObjectHandle w_obj = dict.getKey("/Width");
    QPDFObjectHandle h_obj = dict.getKey("/Height");
    QPDFObjectHandle colorspace_obj = dict.getKey("/ColorSpace");
    if (!(w_obj.isNumber() && h_obj.isNumber())) {
        if (!description.empty()) {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because image dictionary"
                     << " is missing required keys" << std::endl;
            });
        }
        return result;
    }
    QPDFObjectHandle components_obj = dict.getKey("/BitsPerComponent");
    if (!(components_obj.isInteger() && (components_obj.getIntValue() == 8))) {
        QTC::TC("qpdf", "QPDFJob image optimize bits per component");
        if (!description.empty()) {
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
    if (w_obj.isInteger()) {
        w = w_obj.getUIntValueAsUInt();
    } else {
        w = static_cast<JDIMENSION>(w_obj.getNumericValue());
    }
    JDIMENSION h = 0;
    if (h_obj.isInteger()) {
        h = h_obj.getUIntValueAsUInt();
    } else {
        h = static_cast<JDIMENSION>(h_obj.getNumericValue());
    }
    std::string colorspace =
        (colorspace_obj.isName() ? colorspace_obj.getName() : std::string());
    int components = 0;
    J_COLOR_SPACE cs = JCS_UNKNOWN;
    if (colorspace == "/DeviceRGB") {
        components = 3;
        cs = JCS_RGB;
    } else if (colorspace == "/DeviceGray") {
        components = 1;
        cs = JCS_GRAYSCALE;
    } else if (colorspace == "/DeviceCMYK") {
        components = 4;
        cs = JCS_CMYK;
    } else {
        QTC::TC("qpdf", "QPDFJob image optimize colorspace");
        if (!description.empty()) {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because qpdf can't optimize"
                     << " images with this colorspace" << std::endl;
            });
        }
        return result;
    }
    if (((this->oi_min_width > 0) && (w <= this->oi_min_width)) ||
        ((this->oi_min_height > 0) && (h <= this->oi_min_height)) ||
        ((this->oi_min_area > 0) && ((w * h) <= this->oi_min_area))) {
        QTC::TC("qpdf", "QPDFJob image optimize too small");
        if (!description.empty()) {
            o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": " << description
                     << ": not optimizing because image"
                     << " is smaller than requested minimum dimensions"
                     << std::endl;
            });
        }
        return result;
    }

    result = std::make_shared<Pl_DCT>("jpg", next, w, h, components, cs);
    return result;
}

bool
ImageOptimizer::evaluate(std::string const& description)
{
    if (!image.pipeStreamData(0, 0, qpdf_dl_specialized, true)) {
        QTC::TC("qpdf", "QPDFJob image optimize no pipeline");
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": " << description
                 << ": not optimizing because unable to decode data"
                 << " or data already uses DCT" << std::endl;
        });
        return false;
    }
    Pl_Discard d;
    Pl_Count c("count", &d);
    std::shared_ptr<Pipeline> p = makePipeline(description, &c);
    if (p.get() == nullptr) {
        // message issued by makePipeline
        return false;
    }
    if (!image.pipeStreamData(p.get(), 0, qpdf_dl_specialized)) {
        return false;
    }
    long long orig_length = image.getDict().getKey("/Length").getIntValue();
    if (c.getCount() >= orig_length) {
        QTC::TC("qpdf", "QPDFJob image optimize no shrink");
        o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": " << description
                 << ": not optimizing because DCT compression does not"
                 << " reduce image size" << std::endl;
        });
        return false;
    }
    o.doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": " << description
             << ": optimizing image reduces size from " << orig_length << " to "
             << c.getCount() << std::endl;
    });
    return true;
}

void
ImageOptimizer::provideStreamData(int, int, Pipeline* pipeline)
{
    std::shared_ptr<Pipeline> p = makePipeline("", pipeline);
    if (p.get() == nullptr) {
        // Should not be possible
        image.warnIfPossible("unable to create pipeline after previous"
                             " success; image data will be lost");
        pipeline->finish();
        return;
    }
    image.pipeStreamData(p.get(), 0, qpdf_dl_specialized, false, false);
}

QPDFJob::PageSpec::PageSpec(
    std::string const& filename,
    char const* password,
    std::string const& range) :
    filename(filename),
    range(range)
{
    if (password) {
        this->password = QUtil::make_shared_cstr(password);
    }
}

QPDFPageData::QPDFPageData(
    std::string const& filename, QPDF* qpdf, std::string const& range) :
    filename(filename),
    qpdf(qpdf),
    orig_pages(qpdf->getAllPages())
{
    try {
        this->selected_pages = QUtil::parse_numrange(
            range.c_str(), QIntC::to_int(this->orig_pages.size()));
    } catch (std::runtime_error& e) {
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
    this->cout << prefix << ": " << filename
               << ": write progress: " << percentage << "%" << std::endl;
}

// These default values are duplicated in help and docs.
static int constexpr DEFAULT_KEEP_FILES_OPEN_THRESHOLD = 200;
static int constexpr DEFAULT_OI_MIN_WIDTH = 128;
static int constexpr DEFAULT_OI_MIN_HEIGHT = 128;
static int constexpr DEFAULT_OI_MIN_AREA = 16384;
static int constexpr DEFAULT_II_MIN_BYTES = 1024;

QPDFJob::Members::Members() :
    message_prefix("qpdf"),
    warnings(false),
    cout(&std::cout),
    cerr(&std::cerr),
    encryption_status(0),
    verbose(false),
    password(0),
    linearize(false),
    decrypt(false),
    split_pages(0),
    progress(false),
    suppress_warnings(false),
    warnings_exit_zero(false),
    copy_encryption(false),
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
    keep_files_open_threshold(DEFAULT_KEEP_FILES_OPEN_THRESHOLD),
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
    json_version(0),
    test_json_schema(false),
    check(false),
    optimize_images(false),
    externalize_inline_images(false),
    keep_inline_images(false),
    remove_page_labels(false),
    oi_min_width(DEFAULT_OI_MIN_WIDTH),
    oi_min_height(DEFAULT_OI_MIN_HEIGHT),
    oi_min_area(DEFAULT_OI_MIN_AREA),
    ii_min_bytes(DEFAULT_II_MIN_BYTES),
    underlay("underlay"),
    overlay("overlay"),
    under_overlay(0),
    require_outfile(true),
    replace_input(false),
    check_is_encrypted(false),
    check_requires_password(false)
{
}

QPDFJob::QPDFJob() :
    m(new Members())
{
}

void
QPDFJob::usage(std::string const& msg)
{
    throw QPDFUsage(msg);
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
    if (this->m->verbose && (this->m->cout != nullptr)) {
        fn(*(this->m->cout), this->m->message_prefix);
    }
}

std::shared_ptr<QPDFJob::Config>
QPDFJob::config()
{
    return std::shared_ptr<Config>(new Config(*this));
}

std::string
QPDFJob::job_json_schema_v1()
{
    return JOB_SCHEMA_DATA;
}

void
QPDFJob::parseRotationParameter(std::string const& parameter)
{
    std::string angle_str;
    std::string range;
    size_t colon = parameter.find(':');
    int relative = 0;
    if (colon != std::string::npos) {
        if (colon > 0) {
            angle_str = parameter.substr(0, colon);
        }
        if (colon + 1 < parameter.length()) {
            range = parameter.substr(colon + 1);
        }
    } else {
        angle_str = parameter;
    }
    if (angle_str.length() > 0) {
        char first = angle_str.at(0);
        if ((first == '+') || (first == '-')) {
            relative = ((first == '+') ? 1 : -1);
            angle_str = angle_str.substr(1);
        } else if (!QUtil::is_digit(angle_str.at(0))) {
            angle_str = "";
        }
    }
    if (range.empty()) {
        range = "1-z";
    }
    bool range_valid = false;
    try {
        QUtil::parse_numrange(range.c_str(), 0);
        range_valid = true;
    } catch (std::runtime_error const&) {
        // ignore
    }
    if (range_valid &&
        ((angle_str == "0") || (angle_str == "90") || (angle_str == "180") ||
         (angle_str == "270"))) {
        int angle = QUtil::string_to_int(angle_str.c_str());
        if (relative == -1) {
            angle = -angle;
        }
        m->rotations[range] = RotationSpec(angle, (relative != 0));
    } else {
        usage("invalid parameter to rotate: " + parameter);
    }
}

std::vector<int>
QPDFJob::parseNumrange(char const* range, int max)
{
    try {
        return QUtil::parse_numrange(range, max);
    } catch (std::runtime_error& e) {
        usage(e.what());
    }
    return std::vector<int>();
}

void
QPDFJob::run()
{
    checkConfiguration();
    std::shared_ptr<QPDF> pdf_ph;
    try {
        pdf_ph = processFile(m->infilename.get(), m->password.get(), true);
    } catch (QPDFExc& e) {
        if ((e.getErrorCode() == qpdf_e_password) &&
            (m->check_is_encrypted || m->check_requires_password)) {
            // Allow --is-encrypted and --requires-password to
            // work when an incorrect password is supplied.
            this->m->encryption_status =
                qpdf_es_encrypted | qpdf_es_password_incorrect;
            return;
        }
        throw e;
    }
    QPDF& pdf = *pdf_ph;
    if (pdf.isEncrypted()) {
        this->m->encryption_status = qpdf_es_encrypted;
    }

    if (m->check_is_encrypted || m->check_requires_password) {
        return;
    }
    bool other_warnings = false;
    std::vector<std::shared_ptr<QPDF>> page_heap;
    if (!m->page_specs.empty()) {
        handlePageSpecs(pdf, other_warnings, page_heap);
    }
    if (!m->rotations.empty()) {
        handleRotations(pdf);
    }
    handleUnderOverlay(pdf);
    handleTransformations(pdf);

    if (!createsOutput()) {
        doInspection(pdf);
    } else if (m->split_pages) {
        doSplitPages(pdf, other_warnings);
    } else {
        writeOutfile(pdf);
    }
    if (!pdf.getWarnings().empty()) {
        this->m->warnings = true;
    }
    if (this->m->warnings && (!this->m->suppress_warnings)) {
        if (createsOutput()) {
            (*this->m->cerr)
                << this->m->message_prefix
                << ": operation succeeded with warnings;"
                << " resulting file may have some problems" << std::endl;
        } else {
            (*this->m->cerr)
                << this->m->message_prefix
                << ": operation succeeded with warnings" << std::endl;
        }
    }
}

bool
QPDFJob::hasWarnings() const
{
    return this->m->warnings;
}

bool
QPDFJob::createsOutput() const
{
    return ((m->outfilename != nullptr) || m->replace_input);
}

int
QPDFJob::getExitCode() const
{
    if (this->m->check_is_encrypted) {
        if (this->m->encryption_status & qpdf_es_encrypted) {
            QTC::TC("qpdf", "QPDFJob check encrypted encrypted");
            return 0;
        } else {
            QTC::TC("qpdf", "QPDFJob check encrypted not encrypted");
            return EXIT_IS_NOT_ENCRYPTED;
        }
    } else if (this->m->check_requires_password) {
        if (this->m->encryption_status & qpdf_es_encrypted) {
            if (this->m->encryption_status & qpdf_es_password_incorrect) {
                QTC::TC("qpdf", "QPDFJob check password password incorrect");
                return 0;
            } else {
                QTC::TC("qpdf", "QPDFJob check password password correct");
                return EXIT_CORRECT_PASSWORD;
            }
        } else {
            QTC::TC("qpdf", "QPDFJob check password not encrypted");
            return EXIT_IS_NOT_ENCRYPTED;
        }
    }

    if (this->m->warnings && (!this->m->warnings_exit_zero)) {
        return EXIT_WARNING;
    }
    return 0;
}

void
QPDFJob::checkConfiguration()
{
    if (m->replace_input) {
        if (m->outfilename) {
            usage("--replace-input may not be used when"
                  " an output file is specified");
        } else if (m->split_pages) {
            usage("--split-pages may not be used with --replace-input");
        }
    }
    if (m->infilename == 0) {
        usage("an input file name is required");
    } else if (
        m->require_outfile && (m->outfilename == 0) && (!m->replace_input)) {
        usage("an output file name is required; use - for standard output");
    } else if (
        (!m->require_outfile) && ((m->outfilename != 0) || m->replace_input)) {
        usage("no output file may be given for this option");
    }
    if (m->check_requires_password && m->check_is_encrypted) {
        usage("--requires-password and --is-encrypted may not be given"
              " together");
    }

    if (m->encrypt && (!m->allow_insecure) &&
        (m->owner_password.empty() && (!m->user_password.empty()) &&
         (m->keylen == 256))) {
        // Note that empty owner passwords for R < 5 are copied from
        // the user password, so this lack of security is not an issue
        // for those files. Also we are consider only the ability to
        // open the file without a password to be insecure. We are not
        // concerned about whether the viewer enforces security
        // settings when the user and owner password match.
        usage("A PDF with a non-empty user password and an empty owner"
              " password encrypted with a 256-bit key is insecure as it"
              " can be opened without a password. If you really want to"
              " do this, you must also give the --allow-insecure option"
              " before the -- that follows --encrypt.");
    }

    if (m->require_outfile && m->outfilename &&
        (strcmp(m->outfilename.get(), "-") == 0)) {
        if (m->split_pages) {
            usage("--split-pages may not be used when"
                  " writing to standard output");
        }
        if (this->m->verbose) {
            usage("--verbose may not be used when"
                  " writing to standard output");
        }
        if (m->progress) {
            usage("--progress may not be used when"
                  " writing to standard output");
        }
    }

    if ((!m->split_pages) &&
        QUtil::same_file(m->infilename.get(), m->outfilename.get())) {
        QTC::TC("qpdf", "QPDFJob same file error");
        usage("input file and output file are the same;"
              " use --replace-input to intentionally"
              " overwrite the input file");
    }
}

unsigned long
QPDFJob::getEncryptionStatus()
{
    return this->m->encryption_status;
}

void
QPDFJob::setQPDFOptions(QPDF& pdf)
{
    pdf.setOutputStreams(this->m->cout, this->m->cerr);
    if (m->ignore_xref_streams) {
        pdf.setIgnoreXRefStreams(true);
    }
    if (m->suppress_recovery) {
        pdf.setAttemptRecovery(false);
    }
    if (m->password_is_hex_key) {
        pdf.setPasswordIsHexKey(true);
    }
    if (m->suppress_warnings) {
        pdf.setSuppressWarnings(true);
    }
}

static std::string
show_bool(bool v)
{
    return v ? "allowed" : "not allowed";
}

static std::string
show_encryption_method(QPDF::encryption_method_e method)
{
    std::string result = "unknown";
    switch (method) {
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
    // Extract /P from /Encrypt
    int R = 0;
    int P = 0;
    int V = 0;
    QPDF::encryption_method_e stream_method = QPDF::e_unknown;
    QPDF::encryption_method_e string_method = QPDF::e_unknown;
    QPDF::encryption_method_e file_method = QPDF::e_unknown;
    auto& cout = *this->m->cout;
    if (!pdf.isEncrypted(R, P, V, stream_method, string_method, file_method)) {
        cout << "File is not encrypted" << std::endl;
    } else {
        cout << "R = " << R << std::endl;
        cout << "P = " << P << std::endl;
        std::string user_password = pdf.getTrimmedUserPassword();
        std::string encryption_key = pdf.getEncryptionKey();
        cout << "User password = " << user_password << std::endl;
        if (m->show_encryption_key) {
            cout << "Encryption key = " << QUtil::hex_encode(encryption_key)
                 << std::endl;
        }
        if (pdf.ownerPasswordMatched()) {
            cout << "Supplied password is owner password" << std::endl;
        }
        if (pdf.userPasswordMatched()) {
            cout << "Supplied password is user password" << std::endl;
        }
        cout << "extract for accessibility: "
             << show_bool(pdf.allowAccessibility()) << std::endl
             << "extract for any purpose: " << show_bool(pdf.allowExtractAll())
             << std::endl
             << "print low resolution: " << show_bool(pdf.allowPrintLowRes())
             << std::endl
             << "print high resolution: " << show_bool(pdf.allowPrintHighRes())
             << std::endl
             << "modify document assembly: "
             << show_bool(pdf.allowModifyAssembly()) << std::endl
             << "modify forms: " << show_bool(pdf.allowModifyForm())
             << std::endl
             << "modify annotations: " << show_bool(pdf.allowModifyAnnotation())
             << std::endl
             << "modify other: " << show_bool(pdf.allowModifyOther())
             << std::endl
             << "modify anything: " << show_bool(pdf.allowModifyAll())
             << std::endl;
        if (V >= 4) {
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
    // Code below may set okay to false but not to true.
    // We assume okay until we prove otherwise but may
    // continue to perform additional checks after finding
    // errors.
    bool okay = true;
    bool warnings = false;
    auto& cout = *this->m->cout;
    cout << "checking " << m->infilename << std::endl;
    try {
        int extension_level = pdf.getExtensionLevel();
        cout << "PDF Version: " << pdf.getPDFVersion();
        if (extension_level > 0) {
            cout << " extension level " << pdf.getExtensionLevel();
        }
        cout << std::endl;
        showEncryption(pdf);
        if (pdf.isLinearized()) {
            cout << "File is linearized\n";
            // any errors or warnings are reported by
            // checkLinearization(). We treat all issues reported here
            // as warnings.
            if (!pdf.checkLinearization()) {
                warnings = true;
            }
        } else {
            cout << "File is not linearized\n";
        }

        // Write the file to nowhere, uncompressing
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
        for (auto& page: pages) {
            ++pageno;
            try {
                page.parseContents(&discard_contents);
            } catch (QPDFExc& e) {
                okay = false;
                *(this->m->cerr) << "ERROR: page " << pageno << ": " << e.what()
                                 << std::endl;
            }
        }
    } catch (std::exception& e) {
        (*this->m->cerr) << "ERROR: " << e.what() << std::endl;
        okay = false;
    }
    if (!okay) {
        throw std::runtime_error("errors detected");
    }

    if ((!pdf.getWarnings().empty()) || warnings) {
        this->m->warnings = true;
    } else {
        *(this->m->cout) << "No syntax or stream encoding errors"
                         << " found; the file may still contain" << std::endl
                         << "errors that qpdf cannot detect" << std::endl;
    }
}

void
QPDFJob::doShowObj(QPDF& pdf)
{
    QPDFObjectHandle obj;
    if (m->show_trailer) {
        obj = pdf.getTrailer();
    } else {
        obj = pdf.getObjectByID(m->show_obj, m->show_gen);
    }
    bool error = false;
    if (obj.isStream()) {
        if (m->show_raw_stream_data || m->show_filtered_stream_data) {
            bool filter = m->show_filtered_stream_data;
            if (filter && (!obj.pipeStreamData(0, 0, qpdf_dl_all))) {
                QTC::TC("qpdf", "QPDFJob unable to filter");
                obj.warnIfPossible("unable to filter stream data");
                error = true;
            } else {
                QUtil::binary_stdout();
                Pl_StdioFile out("stdout", stdout);
                obj.pipeStreamData(
                    &out,
                    (filter && m->normalize) ? qpdf_ef_normalize : 0,
                    filter ? qpdf_dl_all : qpdf_dl_none);
            }
        } else {
            *(this->m->cout) << "Object is stream.  Dictionary:" << std::endl
                             << obj.getDict().unparseResolved() << std::endl;
        }
    } else {
        *(this->m->cout) << obj.unparseResolved() << std::endl;
    }
    if (error) {
        throw std::runtime_error(
            "unable to get object " + obj.getObjGen().unparse());
    }
}

void
QPDFJob::doShowPages(QPDF& pdf)
{
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    int pageno = 0;
    auto& cout = *this->m->cout;
    for (auto& ph: pages) {
        QPDFObjectHandle page = ph.getObjectHandle();
        ++pageno;

        cout << "page " << pageno << ": " << page.getObjectID() << " "
             << page.getGeneration() << " R" << std::endl;
        if (m->show_page_images) {
            std::map<std::string, QPDFObjectHandle> images = ph.getImages();
            if (!images.empty()) {
                cout << "  images:" << std::endl;
                for (auto const& iter2: images) {
                    std::string const& name = iter2.first;
                    QPDFObjectHandle image = iter2.second;
                    QPDFObjectHandle dict = image.getDict();
                    int width = dict.getKey("/Width").getIntValueAsInt();
                    int height = dict.getKey("/Height").getIntValueAsInt();
                    cout << "    " << name << ": " << image.unparse() << ", "
                         << width << " x " << height << std::endl;
                }
            }
        }

        cout << "  content:" << std::endl;
        std::vector<QPDFObjectHandle> content = ph.getPageContents();
        for (auto& iter2: content) {
            cout << "    " << iter2.unparse() << std::endl;
        }
    }
}

void
QPDFJob::doListAttachments(QPDF& pdf)
{
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    if (efdh.hasEmbeddedFiles()) {
        for (auto const& i: efdh.getEmbeddedFiles()) {
            std::string const& key = i.first;
            auto efoh = i.second;
            *(this->m->cout)
                << key << " -> " << efoh->getEmbeddedFileStream().getObjGen()
                << std::endl;
            doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                auto desc = efoh->getDescription();
                if (!desc.empty()) {
                    cout << "  description: " << desc << std::endl;
                }
                cout << "  preferred name: " << efoh->getFilename()
                     << std::endl;
                cout << "  all names:" << std::endl;
                for (auto const& i2: efoh->getFilenames()) {
                    cout << "    " << i2.first << " -> " << i2.second
                         << std::endl;
                }
                cout << "  all data streams:" << std::endl;
                for (auto i2: efoh->getEmbeddedFileStreams().ditems()) {
                    cout << "    " << i2.first << " -> "
                         << i2.second.getObjGen() << std::endl;
                }
            });
        }
    } else {
        *(this->m->cout) << m->infilename << " has no embedded files"
                         << std::endl;
    }
}

void
QPDFJob::doShowAttachment(QPDF& pdf)
{
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    auto fs = efdh.getEmbeddedFile(m->attachment_to_show);
    if (!fs) {
        throw std::runtime_error(
            "attachment " + m->attachment_to_show + " not found");
    }
    auto efs = fs->getEmbeddedFileStream();
    QUtil::binary_stdout();
    Pl_StdioFile out("stdout", stdout);
    efs.pipeStreamData(&out, 0, qpdf_dl_all);
}

void
QPDFJob::parse_object_id(
    std::string const& objspec, bool& trailer, int& obj, int& gen)
{
    if (objspec == "trailer") {
        trailer = true;
    } else {
        trailer = false;
        obj = QUtil::string_to_int(objspec.c_str());
        size_t comma = objspec.find(',');
        if ((comma != std::string::npos) && (comma + 1 < objspec.length())) {
            gen = QUtil::string_to_int(
                objspec.substr(1 + comma, std::string::npos).c_str());
        }
    }
}

std::set<QPDFObjGen>
QPDFJob::getWantedJSONObjects()
{
    std::set<QPDFObjGen> wanted_og;
    for (auto const& iter: m->json_objects) {
        bool trailer;
        int obj = 0;
        int gen = 0;
        parse_object_id(iter, trailer, obj, gen);
        if (obj) {
            wanted_og.insert(QPDFObjGen(obj, gen));
        }
    }
    return wanted_og;
}

void
QPDFJob::doJSONObjects(QPDF& pdf, JSON& j)
{
    bool all_objects = m->json_objects.empty();
    std::set<QPDFObjGen> wanted_og = getWantedJSONObjects();
    JSON j_objects = j.addDictionaryMember("objects", JSON::makeDictionary());
    if (all_objects || m->json_objects.count("trailer")) {
        j_objects.addDictionaryMember(
            "trailer", pdf.getTrailer().getJSON(true));
    }
    std::vector<QPDFObjectHandle> objects = pdf.getAllObjects();
    for (auto& obj: objects) {
        if (all_objects || wanted_og.count(obj.getObjGen())) {
            j_objects.addDictionaryMember(obj.unparse(), obj.getJSON(true));
        }
    }
}

void
QPDFJob::doJSONObjectinfo(QPDF& pdf, JSON& j)
{
    bool all_objects = m->json_objects.empty();
    std::set<QPDFObjGen> wanted_og = getWantedJSONObjects();
    JSON j_objectinfo =
        j.addDictionaryMember("objectinfo", JSON::makeDictionary());
    for (auto& obj: pdf.getAllObjects()) {
        if (all_objects || wanted_og.count(obj.getObjGen())) {
            auto j_details = j_objectinfo.addDictionaryMember(
                obj.unparse(), JSON::makeDictionary());
            auto j_stream =
                j_details.addDictionaryMember("stream", JSON::makeDictionary());
            bool is_stream = obj.isStream();
            j_stream.addDictionaryMember("is", JSON::makeBool(is_stream));
            j_stream.addDictionaryMember(
                "length",
                (is_stream ? obj.getDict().getKey("/Length").getJSON(true)
                           : JSON::makeNull()));
            j_stream.addDictionaryMember(
                "filter",
                (is_stream ? obj.getDict().getKey("/Filter").getJSON(true)
                           : JSON::makeNull()));
        }
    }
}

void
QPDFJob::doJSONPages(QPDF& pdf, JSON& j)
{
    JSON j_pages = j.addDictionaryMember("pages", JSON::makeArray());
    QPDFPageDocumentHelper pdh(pdf);
    QPDFPageLabelDocumentHelper pldh(pdf);
    QPDFOutlineDocumentHelper odh(pdf);
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    int pageno = -1;
    for (auto& ph: pages) {
        ++pageno;
        JSON j_page = j_pages.addArrayElement(JSON::makeDictionary());
        QPDFObjectHandle page = ph.getObjectHandle();
        j_page.addDictionaryMember("object", page.getJSON());
        JSON j_images = j_page.addDictionaryMember("images", JSON::makeArray());
        std::map<std::string, QPDFObjectHandle> images = ph.getImages();
        for (auto const& iter2: images) {
            JSON j_image = j_images.addArrayElement(JSON::makeDictionary());
            j_image.addDictionaryMember("name", JSON::makeString(iter2.first));
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
            j_image.addDictionaryMember("filter", filters.getJSON());
            QPDFObjectHandle decode_parms = dict.getKey("/DecodeParms");
            QPDFObjectHandle dp_array;
            if (decode_parms.isArray()) {
                dp_array = decode_parms;
            } else {
                dp_array = QPDFObjectHandle::newArray();
                for (int i = 0; i < filters.getArrayNItems(); ++i) {
                    dp_array.appendItem(decode_parms);
                }
            }
            j_image.addDictionaryMember("decodeparms", dp_array.getJSON());
            j_image.addDictionaryMember(
                "filterable",
                JSON::makeBool(
                    image.pipeStreamData(0, 0, m->decode_level, true)));
        }
        j_page.addDictionaryMember("images", j_images);
        JSON j_contents =
            j_page.addDictionaryMember("contents", JSON::makeArray());
        std::vector<QPDFObjectHandle> content = ph.getPageContents();
        for (auto& iter2: content) {
            j_contents.addArrayElement(iter2.getJSON());
        }
        j_page.addDictionaryMember(
            "label", pldh.getLabelForPage(pageno).getJSON());
        JSON j_outlines =
            j_page.addDictionaryMember("outlines", JSON::makeArray());
        std::vector<QPDFOutlineObjectHelper> outlines =
            odh.getOutlinesForPage(page.getObjGen());
        for (auto& oiter: outlines) {
            JSON j_outline = j_outlines.addArrayElement(JSON::makeDictionary());
            j_outline.addDictionaryMember(
                "object", oiter.getObjectHandle().getJSON());
            j_outline.addDictionaryMember(
                "title", JSON::makeString(oiter.getTitle()));
            j_outline.addDictionaryMember(
                "dest", oiter.getDest().getJSON(true));
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
    if (pldh.hasPageLabels()) {
        std::vector<QPDFObjectHandle> labels;
        pldh.getLabelsForPageRange(
            0, QIntC::to_int(pages.size()) - 1, 0, labels);
        for (auto iter = labels.begin(); iter != labels.end(); ++iter) {
            auto next = iter;
            ++next;
            if (next == labels.end()) {
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

static void
add_outlines_to_json(
    std::vector<QPDFOutlineObjectHelper> outlines,
    JSON& j,
    std::map<QPDFObjGen, int>& page_numbers)
{
    for (auto& ol: outlines) {
        JSON jo = j.addArrayElement(JSON::makeDictionary());
        jo.addDictionaryMember("object", ol.getObjectHandle().getJSON());
        jo.addDictionaryMember("title", JSON::makeString(ol.getTitle()));
        jo.addDictionaryMember("dest", ol.getDest().getJSON(true));
        jo.addDictionaryMember("open", JSON::makeBool(ol.getCount() >= 0));
        QPDFObjectHandle page = ol.getDestPage();
        JSON j_destpage = JSON::makeNull();
        if (page.isIndirect()) {
            QPDFObjGen og = page.getObjGen();
            if (page_numbers.count(og)) {
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
    for (auto const& ph: pages) {
        QPDFObjectHandle oh = ph.getObjectHandle();
        page_numbers[oh.getObjGen()] = ++n;
    }

    JSON j_outlines = j.addDictionaryMember("outlines", JSON::makeArray());
    QPDFOutlineDocumentHelper odh(pdf);
    add_outlines_to_json(odh.getTopLevelOutlines(), j_outlines, page_numbers);
}

void
QPDFJob::doJSONAcroform(QPDF& pdf, JSON& j)
{
    JSON j_acroform = j.addDictionaryMember("acroform", JSON::makeDictionary());
    QPDFAcroFormDocumentHelper afdh(pdf);
    j_acroform.addDictionaryMember(
        "hasacroform", JSON::makeBool(afdh.hasAcroForm()));
    j_acroform.addDictionaryMember(
        "needappearances", JSON::makeBool(afdh.getNeedAppearances()));
    JSON j_fields = j_acroform.addDictionaryMember("fields", JSON::makeArray());
    QPDFPageDocumentHelper pdh(pdf);
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    int pagepos1 = 0;
    for (auto const& page: pages) {
        ++pagepos1;
        std::vector<QPDFAnnotationObjectHelper> annotations =
            afdh.getWidgetAnnotationsForPage(page);
        for (auto& aoh: annotations) {
            QPDFFormFieldObjectHelper ffh = afdh.getFieldForAnnotation(aoh);
            JSON j_field = j_fields.addArrayElement(JSON::makeDictionary());
            j_field.addDictionaryMember(
                "object", ffh.getObjectHandle().getJSON());
            j_field.addDictionaryMember(
                "parent", ffh.getObjectHandle().getKey("/Parent").getJSON());
            j_field.addDictionaryMember(
                "pageposfrom1", JSON::makeInt(pagepos1));
            j_field.addDictionaryMember(
                "fieldtype", JSON::makeString(ffh.getFieldType()));
            j_field.addDictionaryMember(
                "fieldflags", JSON::makeInt(ffh.getFlags()));
            j_field.addDictionaryMember(
                "fullname", JSON::makeString(ffh.getFullyQualifiedName()));
            j_field.addDictionaryMember(
                "partialname", JSON::makeString(ffh.getPartialName()));
            j_field.addDictionaryMember(
                "alternativename", JSON::makeString(ffh.getAlternativeName()));
            j_field.addDictionaryMember(
                "mappingname", JSON::makeString(ffh.getMappingName()));
            j_field.addDictionaryMember("value", ffh.getValue().getJSON());
            j_field.addDictionaryMember(
                "defaultvalue", ffh.getDefaultValue().getJSON());
            j_field.addDictionaryMember(
                "quadding", JSON::makeInt(ffh.getQuadding()));
            j_field.addDictionaryMember(
                "ischeckbox", JSON::makeBool(ffh.isCheckbox()));
            j_field.addDictionaryMember(
                "isradiobutton", JSON::makeBool(ffh.isRadioButton()));
            j_field.addDictionaryMember(
                "ischoice", JSON::makeBool(ffh.isChoice()));
            j_field.addDictionaryMember("istext", JSON::makeBool(ffh.isText()));
            JSON j_choices =
                j_field.addDictionaryMember("choices", JSON::makeArray());
            std::vector<std::string> choices = ffh.getChoices();
            for (auto const& choice: choices) {
                j_choices.addArrayElement(JSON::makeString(choice));
            }
            JSON j_annot = j_field.addDictionaryMember(
                "annotation", JSON::makeDictionary());
            j_annot.addDictionaryMember(
                "object", aoh.getObjectHandle().getJSON());
            j_annot.addDictionaryMember(
                "appearancestate", JSON::makeString(aoh.getAppearanceState()));
            j_annot.addDictionaryMember(
                "annotationflags", JSON::makeInt(aoh.getFlags()));
        }
    }
}

void
QPDFJob::doJSONEncrypt(QPDF& pdf, JSON& j)
{
    int R = 0;
    int P = 0;
    int V = 0;
    QPDF::encryption_method_e stream_method = QPDF::e_none;
    QPDF::encryption_method_e string_method = QPDF::e_none;
    QPDF::encryption_method_e file_method = QPDF::e_none;
    bool is_encrypted =
        pdf.isEncrypted(R, P, V, stream_method, string_method, file_method);
    JSON j_encrypt = j.addDictionaryMember("encrypt", JSON::makeDictionary());
    j_encrypt.addDictionaryMember("encrypted", JSON::makeBool(is_encrypted));
    j_encrypt.addDictionaryMember(
        "userpasswordmatched",
        JSON::makeBool(is_encrypted && pdf.userPasswordMatched()));
    j_encrypt.addDictionaryMember(
        "ownerpasswordmatched",
        JSON::makeBool(is_encrypted && pdf.ownerPasswordMatched()));
    JSON j_capabilities =
        j_encrypt.addDictionaryMember("capabilities", JSON::makeDictionary());
    j_capabilities.addDictionaryMember(
        "accessibility", JSON::makeBool(pdf.allowAccessibility()));
    j_capabilities.addDictionaryMember(
        "extract", JSON::makeBool(pdf.allowExtractAll()));
    j_capabilities.addDictionaryMember(
        "printlow", JSON::makeBool(pdf.allowPrintLowRes()));
    j_capabilities.addDictionaryMember(
        "printhigh", JSON::makeBool(pdf.allowPrintHighRes()));
    j_capabilities.addDictionaryMember(
        "modifyassembly", JSON::makeBool(pdf.allowModifyAssembly()));
    j_capabilities.addDictionaryMember(
        "modifyforms", JSON::makeBool(pdf.allowModifyForm()));
    // Typo will be fixed for json v2
    /* cSpell:ignore moddifyannotations */
    j_capabilities.addDictionaryMember(
        "moddifyannotations", JSON::makeBool(pdf.allowModifyAnnotation()));
    j_capabilities.addDictionaryMember(
        "modifyother", JSON::makeBool(pdf.allowModifyOther()));
    j_capabilities.addDictionaryMember(
        "modify", JSON::makeBool(pdf.allowModifyAll()));
    JSON j_parameters =
        j_encrypt.addDictionaryMember("parameters", JSON::makeDictionary());
    j_parameters.addDictionaryMember("R", JSON::makeInt(R));
    j_parameters.addDictionaryMember("V", JSON::makeInt(V));
    j_parameters.addDictionaryMember("P", JSON::makeInt(P));
    int bits = 0;
    JSON key = JSON::makeNull();
    if (is_encrypted) {
        std::string encryption_key = pdf.getEncryptionKey();
        bits = QIntC::to_int(encryption_key.length() * 8);
        if (m->show_encryption_key) {
            key = JSON::makeString(QUtil::hex_encode(encryption_key));
        }
    }
    j_parameters.addDictionaryMember("bits", JSON::makeInt(bits));
    j_parameters.addDictionaryMember("key", key);
    auto fix_method = [is_encrypted](QPDF::encryption_method_e& m) {
        if (is_encrypted && m == QPDF::e_none) {
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
    if ((stream_method == string_method) && (stream_method == file_method)) {
        s_overall_method = s_stream_method;
    } else {
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
    JSON j_attachments =
        j.addDictionaryMember("attachments", JSON::makeDictionary());
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    for (auto const& iter: efdh.getEmbeddedFiles()) {
        std::string const& key = iter.first;
        auto fsoh = iter.second;
        auto j_details =
            j_attachments.addDictionaryMember(key, JSON::makeDictionary());
        j_details.addDictionaryMember(
            "filespec", JSON::makeString(fsoh->getObjectHandle().unparse()));
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
        "version",
        JSON::makeString(
            "JSON format serial number; increased for non-compatible changes"));
    JSON j_params = schema.addDictionaryMember("parameters", JSON::parse(R"({
  "decodelevel": "decode level used to determine stream filterability"
})"));

    bool all_keys = ((keys == 0) || keys->empty());

    // The list of selectable top-level keys id duplicated in the
    // following places: job.yml, QPDFJob::json_schema, and
    // QPDFJob::doJSON.
    if (all_keys || keys->count("objects")) {
        schema.addDictionaryMember("objects", JSON::parse(R"({
  "<n n R|trailer>": "json representation of object"
})"));
    }
    if (all_keys || keys->count("objectinfo")) {
        JSON objectinfo =
            schema.addDictionaryMember("objectinfo", JSON::parse(R"({
  "<object-id>": {
    "stream": {
      "filter": "if stream, its filters, otherwise null",
      "is": "whether the object is a stream",
      "length": "if stream, its length, otherwise null"
    }
  }
})"));
    }
    if (all_keys || keys->count("pages")) {
        JSON page = schema.addDictionaryMember("pages", JSON::parse(R"([
  {
    "contents": [
      "reference to each content stream"
    ],
    "images": [
      {
        "bitspercomponent": "bits per component",
        "colorspace": "color space",
        "decodeparms": [
          "decode parameters for image data"
        ],
        "filter": [
          "filters applied to image data"
        ],
        "filterable": "whether image data can be decoded using the decode level qpdf was invoked with",
        "height": "image height",
        "name": "name of image in XObject table",
        "object": "reference to image stream",
        "width": "image width"
      }
    ],
    "label": "page label dictionary, or null if none",
    "object": "reference to original page object",
    "outlines": [
      {
        "dest": "outline destination dictionary",
        "object": "reference to outline that targets this page",
        "title": "outline title"
      }
    ],
    "pageposfrom1": "position of page in document numbering from 1"
  }
])"));
    }
    if (all_keys || keys->count("pagelabels")) {
        JSON labels = schema.addDictionaryMember("pagelabels", JSON::parse(R"([
  {
    "index": "starting page position starting from zero",
    "label": "page label dictionary"
  }
])"));
    }
    if (all_keys || keys->count("outlines")) {
        JSON outlines = schema.addDictionaryMember("outlines", JSON::parse(R"([
  {
    "dest": "outline destination dictionary",
    "destpageposfrom1": "position of destination page in document numbered from 1; null if not known",
    "kids": "array of descendent outlines",
    "object": "reference to this outline",
    "open": "whether the outline is displayed expanded",
    "title": "outline title"
  }
])"));
    }
    if (all_keys || keys->count("acroform")) {
        JSON acroform = schema.addDictionaryMember("acroform", JSON::parse(R"({
  "fields": [
    {
      "alternativename": "alternative name of field -- this is the one usually shown to users",
      "annotation": {
        "annotationflags": "annotation flags from /F -- see pdf_annotation_flag_e in qpdf/Constants.h",
        "appearancestate": "appearance state -- can be used to determine value for checkboxes and radio buttons",
        "object": "reference to the annotation object"
      },
      "choices": "for choices fields, the list of choices presented to the user",
      "defaultvalue": "default value of field",
      "fieldflags": "form field flags from /Ff -- see pdf_form_field_flag_e in qpdf/Constants.h",
      "fieldtype": "field type",
      "fullname": "full name of field",
      "ischeckbox": "whether field is a checkbox",
      "ischoice": "whether field is a list, combo, or dropdown",
      "isradiobutton": "whether field is a radio button -- buttons in a single group share a parent",
      "istext": "whether field is a text field",
      "mappingname": "mapping name of field",
      "object": "reference to this form field",
      "pageposfrom1": "position of containing page numbered from 1",
      "parent": "reference to this field's parent",
      "partialname": "partial name of field",
      "quadding": "field quadding -- number indicating left, center, or right",
      "value": "value of field"
    }
  ],
  "hasacroform": "whether the document has interactive forms",
  "needappearances": "whether the form fields' appearance streams need to be regenerated"
})"));
    }
    if (all_keys || keys->count("encrypt")) {
        JSON encrypt = schema.addDictionaryMember("encrypt", JSON::parse(R"({
  "capabilities": {
    "accessibility": "allow extraction for accessibility?",
    "extract": "allow extraction?",
    "moddifyannotations": "allow modifying annotations?",
    "modify": "allow all modifications?",
    "modifyassembly": "allow modifying document assembly?",
    "modifyforms": "allow modifying forms?",
    "modifyother": "allow other modifications?",
    "printhigh": "allow high resolution printing?",
    "printlow": "allow low resolution printing?"
  },
  "encrypted": "whether the document is encrypted",
  "ownerpasswordmatched": "whether supplied password matched owner password; always false for non-encrypted files",
  "parameters": {
    "P": "P value from Encrypt dictionary",
    "R": "R value from Encrypt dictionary",
    "V": "V value from Encrypt dictionary",
    "bits": "encryption key bit length",
    "filemethod": "encryption method for attachments",
    "key": "encryption key; will be null unless --show-encryption-key was specified",
    "method": "overall encryption method: none, mixed, RC4, AESv2, AESv3",
    "streammethod": "encryption method for streams",
    "stringmethod": "encryption method for string"
  },
  "userpasswordmatched": "whether supplied password matched user password; always false for non-encrypted files"
})"));
    }
    if (all_keys || keys->count("attachments")) {
        JSON attachments =
            schema.addDictionaryMember("attachments", JSON::parse(R"({
  "<attachment-key>": {
    "filespec": "object containing the file spec",
    "preferredcontents": "most preferred embedded file stream",
    "preferredname": "most preferred file name"
  }
})"));
    }
    return schema;
}

std::string
QPDFJob::json_out_schema_v1()
{
    return json_schema().unparse();
}

void
QPDFJob::doJSON(QPDF& pdf, Pipeline* p)
{
    std::string captured_json;
    std::shared_ptr<Pl_String> pl_str;
    if (this->m->test_json_schema) {
        pl_str = std::make_shared<Pl_String>("capture json", p, captured_json);
        p = pl_str.get();
    }

    JSON j = JSON::makeDictionary();
    // This version is updated every time a non-backward-compatible
    // change is made to the JSON format. Clients of the JSON are to
    // ignore unrecognized keys, so we only update the version of a
    // key disappears or if its value changes meaning.
    j.addDictionaryMember("version", JSON::makeInt(1));
    JSON j_params = j.addDictionaryMember("parameters", JSON::makeDictionary());
    std::string decode_level_str;
    switch (m->decode_level) {
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

    bool all_keys = m->json_keys.empty();
    // The list of selectable top-level keys id duplicated in the
    // following places: job.yml, QPDFJob::json_schema, and
    // QPDFJob::doJSON.
    if (all_keys || m->json_keys.count("pages")) {
        doJSONPages(pdf, j);
    }
    if (all_keys || m->json_keys.count("pagelabels")) {
        doJSONPageLabels(pdf, j);
    }
    if (all_keys || m->json_keys.count("outlines")) {
        doJSONOutlines(pdf, j);
    }
    if (all_keys || m->json_keys.count("acroform")) {
        doJSONAcroform(pdf, j);
    }
    if (all_keys || m->json_keys.count("encrypt")) {
        doJSONEncrypt(pdf, j);
    }
    if (all_keys || m->json_keys.count("attachments")) {
        doJSONAttachments(pdf, j);
    }

    // We do objects and objectinfo last so their information is
    // consistent with repairing the page tree. To see the original
    // file with any page tree problems and the page tree not
    // flattened, select objects/objectinfo without other keys.
    if (all_keys || m->json_keys.count("objects")) {
        doJSONObjects(pdf, j);
    }
    if (all_keys || m->json_keys.count("objectinfo")) {
        doJSONObjectinfo(pdf, j);
    }

    *p << j.unparse() << "\n";

    if (this->m->test_json_schema) {
        // Check against schema
        JSON schema = json_schema(&m->json_keys);
        std::list<std::string> errors;
        JSON captured = JSON::parse(captured_json);
        if (!captured.checkSchema(schema, errors)) {
            *(this->m->cerr) << "QPDFJob didn't create JSON that complies with"
                                " its own rules."
                             << std::endl;
            for (auto const& error: errors) {
                *(this->m->cerr) << error << std::endl;
            }
        }
    }
}

void
QPDFJob::doInspection(QPDF& pdf)
{
    if (m->check) {
        doCheck(pdf);
    }
    if (m->json_version) {
        Pl_OStream os("stdout", *(this->m->cout));
        doJSON(pdf, &os);
        os.finish();
    }
    if (m->show_npages) {
        QTC::TC("qpdf", "QPDFJob npages");
        *(this->m->cout)
            << pdf.getRoot().getKey("/Pages").getKey("/Count").getIntValue()
            << std::endl;
    }
    if (m->show_encryption) {
        showEncryption(pdf);
    }
    if (m->check_linearization) {
        if (!pdf.isLinearized()) {
            *(this->m->cout)
                << m->infilename << " is not linearized" << std::endl;
        } else if (pdf.checkLinearization()) {
            *(this->m->cout)
                << m->infilename << ": no linearization errors" << std::endl;
        } else {
            this->m->warnings = true;
        }
    }
    if (m->show_linearization) {
        if (pdf.isLinearized()) {
            pdf.showLinearizationData();
        } else {
            *(this->m->cout)
                << m->infilename << " is not linearized" << std::endl;
        }
    }
    if (m->show_xref) {
        pdf.showXRefTable();
    }
    if ((m->show_obj > 0) || m->show_trailer) {
        doShowObj(pdf);
    }
    if (m->show_pages) {
        doShowPages(pdf);
    }
    if (m->list_attachments) {
        doListAttachments(pdf);
    }
    if (!m->attachment_to_show.empty()) {
        doShowAttachment(pdf);
    }
    if (!pdf.getWarnings().empty()) {
        this->m->warnings = true;
    }
}

std::shared_ptr<QPDF>
QPDFJob::doProcessOnce(
    std::function<void(QPDF*, char const*)> fn,
    char const* password,
    bool empty,
    bool used_for_input)
{
    auto pdf = std::make_shared<QPDF>();
    setQPDFOptions(*pdf);
    if (empty) {
        pdf->emptyPDF();
    } else {
        fn(pdf.get(), password);
    }
    if (used_for_input) {
        this->m->max_input_version.updateIfGreater(
            pdf->getVersionAsPDFVersion());
    }
    return pdf;
}

std::shared_ptr<QPDF>
QPDFJob::doProcess(
    std::function<void(QPDF*, char const*)> fn,
    char const* password,
    bool empty,
    bool used_for_input)
{
    // If a password has been specified but doesn't work, try other
    // passwords that are equivalent in different character encodings.
    // This makes it possible to open PDF files that were encrypted
    // using incorrect string encodings. For example, if someone used
    // a password encoded in PDF Doc encoding or Windows code page
    // 1252 for an AES-encrypted file or a UTF-8-encoded password on
    // an RC4-encrypted file, or if the password was properly encoded
    // but the password given here was incorrectly encoded, there's a
    // good chance we'd succeed here.

    std::string ptemp;
    if (password && (!m->password_is_hex_key)) {
        if (m->password_mode == QPDFJob::pm_hex_bytes) {
            // Special case: handle --password-mode=hex-bytes for input
            // password as well as output password
            QTC::TC("qpdf", "QPDFJob input password hex-bytes");
            ptemp = QUtil::hex_decode(password);
            password = ptemp.c_str();
        }
    }
    if ((password == 0) || empty || m->password_is_hex_key ||
        m->suppress_password_recovery) {
        // There is no password, or we're not doing recovery, so just
        // do the normal processing with the supplied password.
        return doProcessOnce(fn, password, empty, used_for_input);
    }

    // Get a list of otherwise encoded strings. Keep in scope for this
    // method.
    std::vector<std::string> passwords_str =
        QUtil::possible_repaired_encodings(password);
    // Represent to char const*, as required by the QPDF class.
    std::vector<char const*> passwords;
    for (auto const& iter: passwords_str) {
        passwords.push_back(iter.c_str());
    }
    // We always try the supplied password first because it is the
    // first string returned by possible_repaired_encodings. If there
    // is more than one option, go ahead and put the supplied password
    // at the end so that it's that decoding attempt whose exception
    // is thrown.
    if (passwords.size() > 1) {
        passwords.push_back(password);
    }

    // Try each password. If one works, return the resulting object.
    // If they all fail, throw the exception thrown by the final
    // attempt, which, like the first attempt, will be with the
    // supplied password.
    bool warned = false;
    for (auto iter = passwords.begin(); iter != passwords.end(); ++iter) {
        try {
            return doProcessOnce(fn, *iter, empty, used_for_input);
        } catch (QPDFExc& e) {
            auto next = iter;
            ++next;
            if (next == passwords.end()) {
                throw e;
            }
        }
        if (!warned) {
            warned = true;
            doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
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

std::shared_ptr<QPDF>
QPDFJob::processFile(
    char const* filename, char const* password, bool used_for_input)
{
    auto f1 = std::mem_fn<void(char const*, char const*)>(&QPDF::processFile);
    auto fn =
        std::bind(f1, std::placeholders::_1, filename, std::placeholders::_2);
    return doProcess(fn, password, strcmp(filename, "") == 0, used_for_input);
}

std::shared_ptr<QPDF>
QPDFJob::processInputSource(
    std::shared_ptr<InputSource> is, char const* password, bool used_for_input)
{
    auto f1 = std::mem_fn(&QPDF::processInputSource);
    auto fn = std::bind(f1, std::placeholders::_1, is, std::placeholders::_2);
    return doProcess(fn, password, false, used_for_input);
}

void
QPDFJob::validateUnderOverlay(QPDF& pdf, UnderOverlay* uo)
{
    if (uo->filename.empty()) {
        return;
    }
    QPDFPageDocumentHelper main_pdh(pdf);
    int main_npages = QIntC::to_int(main_pdh.getAllPages().size());
    uo->pdf = processFile(uo->filename.c_str(), uo->password.get(), true);
    QPDFPageDocumentHelper uo_pdh(*(uo->pdf));
    int uo_npages = QIntC::to_int(uo_pdh.getAllPages().size());
    try {
        uo->to_pagenos = QUtil::parse_numrange(uo->to_nr.c_str(), main_npages);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(
            "parsing numeric range for " + uo->which +
            " \"to\" pages: " + e.what());
    }
    try {
        if (uo->from_nr.empty()) {
            QTC::TC("qpdf", "QPDFJob from_nr from repeat_nr");
            uo->from_nr = uo->repeat_nr;
        }
        uo->from_pagenos =
            QUtil::parse_numrange(uo->from_nr.c_str(), uo_npages);
        if (!uo->repeat_nr.empty()) {
            uo->repeat_pagenos =
                QUtil::parse_numrange(uo->repeat_nr.c_str(), uo_npages);
        }
    } catch (std::runtime_error& e) {
        throw std::runtime_error(
            "parsing numeric range for " + uo->which + " file " + uo->filename +
            ": " + e.what());
    }
}

static QPDFAcroFormDocumentHelper*
get_afdh_for_qpdf(
    std::map<unsigned long long, std::shared_ptr<QPDFAcroFormDocumentHelper>>&
        afdh_map,
    QPDF* q)
{
    auto uid = q->getUniqueId();
    if (!afdh_map.count(uid)) {
        afdh_map[uid] = std::make_shared<QPDFAcroFormDocumentHelper>(*q);
    }
    return afdh_map[uid].get();
}

void
QPDFJob::doUnderOverlayForPage(
    QPDF& pdf,
    UnderOverlay& uo,
    std::map<int, std::vector<int>>& pagenos,
    size_t page_idx,
    std::map<int, QPDFObjectHandle>& fo,
    std::vector<QPDFPageObjectHelper>& pages,
    QPDFPageObjectHelper& dest_page,
    bool before)
{
    int pageno = 1 + QIntC::to_int(page_idx);
    if (!pagenos.count(pageno)) {
        return;
    }

    std::map<unsigned long long, std::shared_ptr<QPDFAcroFormDocumentHelper>>
        afdh;
    auto make_afdh = [&](QPDFPageObjectHelper& ph) {
        QPDF* q = ph.getObjectHandle().getOwningQPDF();
        return get_afdh_for_qpdf(afdh, q);
    };
    auto dest_afdh = make_afdh(dest_page);

    std::string content;
    int min_suffix = 1;
    QPDFObjectHandle resources = dest_page.getAttribute("/Resources", true);
    if (!resources.isDictionary()) {
        QTC::TC("qpdf", "QPDFJob overlay page with no resources");
        resources = dest_page.getObjectHandle().replaceKeyAndGet(
            "/Resources", QPDFObjectHandle::newDictionary());
    }
    for (int from_pageno: pagenos[pageno]) {
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << "    " << uo.which << " " << from_pageno << std::endl;
        });
        auto from_page = pages.at(QIntC::to_size(from_pageno - 1));
        if (0 == fo.count(from_pageno)) {
            fo[from_pageno] =
                pdf.copyForeignObject(from_page.getFormXObjectForPage());
        }

        // If the same page is overlaid or underlaid multiple times,
        // we'll generate multiple names for it, but that's harmless
        // and also a pretty goofy case that's not worth coding
        // around.
        std::string name = resources.getUniqueResourceName("/Fx", min_suffix);
        QPDFMatrix cm;
        std::string new_content = dest_page.placeFormXObject(
            fo[from_pageno],
            name,
            dest_page.getTrimBox().getArrayAsRectangle(),
            cm);
        dest_page.copyAnnotations(
            from_page, cm, dest_afdh, make_afdh(from_page));
        if (!new_content.empty()) {
            resources.mergeResources("<< /XObject << >> >>"_qpdf);
            auto xobject = resources.getKey("/XObject");
            if (xobject.isDictionary()) {
                xobject.replaceKey(name, fo[from_pageno]);
            }
            ++min_suffix;
            content += new_content;
        }
    }
    if (!content.empty()) {
        if (before) {
            dest_page.addPageContents(
                QPDFObjectHandle::newStream(&pdf, content), true);
        } else {
            dest_page.addPageContents(
                QPDFObjectHandle::newStream(&pdf, "q\n"), true);
            dest_page.addPageContents(
                QPDFObjectHandle::newStream(&pdf, "\nQ\n" + content), false);
        }
    }
}

void
QPDFJob::getUOPagenos(
    QPDFJob::UnderOverlay& uo, std::map<int, std::vector<int>>& pagenos)
{
    size_t idx = 0;
    size_t from_size = uo.from_pagenos.size();
    size_t repeat_size = uo.repeat_pagenos.size();
    for (int to_pageno: uo.to_pagenos) {
        if (idx < from_size) {
            pagenos[to_pageno].push_back(uo.from_pagenos.at(idx));
        } else if (repeat_size) {
            pagenos[to_pageno].push_back(
                uo.repeat_pagenos.at((idx - from_size) % repeat_size));
        }
        ++idx;
    }
}

void
QPDFJob::handleUnderOverlay(QPDF& pdf)
{
    validateUnderOverlay(pdf, &m->underlay);
    validateUnderOverlay(pdf, &m->overlay);
    if ((nullptr == m->underlay.pdf.get()) &&
        (nullptr == m->overlay.pdf.get())) {
        return;
    }
    std::map<int, std::vector<int>> underlay_pagenos;
    getUOPagenos(m->underlay, underlay_pagenos);
    std::map<int, std::vector<int>> overlay_pagenos;
    getUOPagenos(m->overlay, overlay_pagenos);
    std::map<int, QPDFObjectHandle> underlay_fo;
    std::map<int, QPDFObjectHandle> overlay_fo;
    std::vector<QPDFPageObjectHelper> upages;
    if (m->underlay.pdf.get()) {
        upages = QPDFPageDocumentHelper(*(m->underlay.pdf)).getAllPages();
    }
    std::vector<QPDFPageObjectHelper> opages;
    if (m->overlay.pdf.get()) {
        opages = QPDFPageDocumentHelper(*(m->overlay.pdf)).getAllPages();
    }

    QPDFPageDocumentHelper main_pdh(pdf);
    std::vector<QPDFPageObjectHelper> main_pages = main_pdh.getAllPages();
    size_t main_npages = main_pages.size();
    doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": processing underlay/overlay" << std::endl;
    });
    for (size_t i = 0; i < main_npages; ++i) {
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << "  page " << 1 + i << std::endl;
        });
        doUnderOverlayForPage(
            pdf,
            m->underlay,
            underlay_pagenos,
            i,
            underlay_fo,
            upages,
            main_pages.at(i),
            true);
        doUnderOverlayForPage(
            pdf,
            m->overlay,
            overlay_pagenos,
            i,
            overlay_fo,
            opages,
            main_pages.at(i),
            false);
    }
}

static void
maybe_set_pagemode(QPDF& pdf, std::string const& pagemode)
{
    auto root = pdf.getRoot();
    if (root.getKey("/PageMode").isNull()) {
        root.replaceKey("/PageMode", QPDFObjectHandle::newName(pagemode));
    }
}

void
QPDFJob::addAttachments(QPDF& pdf)
{
    maybe_set_pagemode(pdf, "/UseAttachments");
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    std::vector<std::string> duplicated_keys;
    for (auto const& to_add: m->attachments_to_add) {
        if ((!to_add.replace) && efdh.getEmbeddedFile(to_add.key)) {
            duplicated_keys.push_back(to_add.key);
            continue;
        }

        auto fs = QPDFFileSpecObjectHelper::createFileSpec(
            pdf, to_add.filename, to_add.path);
        if (!to_add.description.empty()) {
            fs.setDescription(to_add.description);
        }
        auto efs = QPDFEFStreamObjectHelper(fs.getEmbeddedFileStream());
        efs.setCreationDate(to_add.creationdate).setModDate(to_add.moddate);
        if (!to_add.mimetype.empty()) {
            efs.setSubtype(to_add.mimetype);
        }

        efdh.replaceEmbeddedFile(to_add.key, fs);
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": attached " << to_add.path << " as "
                 << to_add.filename << " with key " << to_add.key << std::endl;
        });
    }

    if (!duplicated_keys.empty()) {
        std::string message;
        for (auto const& k: duplicated_keys) {
            if (!message.empty()) {
                message += ", ";
            }
            message += k;
        }
        message = pdf.getFilename() +
            " already has attachments with the following keys: " + message +
            "; use --replace to replace or --key to specify a different key";
        throw std::runtime_error(message);
    }
}

void
QPDFJob::copyAttachments(QPDF& pdf)
{
    maybe_set_pagemode(pdf, "/UseAttachments");
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    std::vector<std::string> duplicates;
    for (auto const& to_copy: m->attachments_to_copy) {
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": copying attachments from " << to_copy.path
                 << std::endl;
        });
        auto other =
            processFile(to_copy.path.c_str(), to_copy.password.c_str(), false);
        QPDFEmbeddedFileDocumentHelper other_efdh(*other);
        auto other_attachments = other_efdh.getEmbeddedFiles();
        for (auto const& iter: other_attachments) {
            std::string new_key = to_copy.prefix + iter.first;
            if (efdh.getEmbeddedFile(new_key)) {
                duplicates.push_back(
                    "file: " + to_copy.path + ", key: " + new_key);
            } else {
                auto new_fs_oh =
                    pdf.copyForeignObject(iter.second->getObjectHandle());
                efdh.replaceEmbeddedFile(
                    new_key, QPDFFileSpecObjectHelper(new_fs_oh));
                doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                    cout << "  " << iter.first << " -> " << new_key
                         << std::endl;
                });
            }
        }

        if (other->anyWarnings()) {
            this->m->warnings = true;
        }
    }

    if (!duplicates.empty()) {
        std::string message;
        for (auto const& i: duplicates) {
            if (!message.empty()) {
                message += "; ";
            }
            message += i;
        }
        message = pdf.getFilename() +
            " already has attachments with keys that conflict with"
            " attachments from other files: " +
            message +
            ". Use --prefix with --copy-attachments-from"
            " or manually copy individual attachments.";
        throw std::runtime_error(message);
    }
}

void
QPDFJob::handleTransformations(QPDF& pdf)
{
    QPDFPageDocumentHelper dh(pdf);
    std::shared_ptr<QPDFAcroFormDocumentHelper> afdh;
    auto make_afdh = [&]() {
        if (!afdh.get()) {
            afdh = std::make_shared<QPDFAcroFormDocumentHelper>(pdf);
        }
    };
    if (m->externalize_inline_images ||
        (m->optimize_images && (!m->keep_inline_images))) {
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (auto& ph: pages) {
            ph.externalizeInlineImages(m->ii_min_bytes);
        }
    }
    if (m->optimize_images) {
        int pageno = 0;
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (auto& ph: pages) {
            ++pageno;
            QPDFObjectHandle page = ph.getObjectHandle();
            std::map<std::string, QPDFObjectHandle> images = ph.getImages();
            for (auto& iter2: images) {
                std::string name = iter2.first;
                QPDFObjectHandle& image = iter2.second;
                ImageOptimizer* io = new ImageOptimizer(
                    *this,
                    m->oi_min_width,
                    m->oi_min_height,
                    m->oi_min_area,
                    image);
                std::shared_ptr<QPDFObjectHandle::StreamDataProvider> sdp(io);
                if (io->evaluate(
                        "image " + name + " on page " +
                        QUtil::int_to_string(pageno))) {
                    QPDFObjectHandle new_image =
                        QPDFObjectHandle::newStream(&pdf);
                    new_image.replaceDict(image.getDict().shallowCopy());
                    new_image.replaceStreamData(
                        sdp,
                        QPDFObjectHandle::newName("/DCTDecode"),
                        QPDFObjectHandle::newNull());
                    ph.getAttribute("/Resources", true)
                        .getKey("/XObject")
                        .replaceKey(name, new_image);
                }
            }
        }
    }
    if (m->generate_appearances) {
        make_afdh();
        afdh->generateAppearancesIfNeeded();
    }
    if (m->flatten_annotations) {
        dh.flattenAnnotations(
            m->flatten_annotations_required, m->flatten_annotations_forbidden);
    }
    if (m->coalesce_contents) {
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (auto& page: pages) {
            page.coalesceContentStreams();
        }
    }
    if (m->flatten_rotation) {
        make_afdh();
        for (auto& page: dh.getAllPages()) {
            page.flattenRotation(afdh.get());
        }
    }
    if (m->remove_page_labels) {
        pdf.getRoot().removeKey("/PageLabels");
    }
    if (!m->attachments_to_remove.empty()) {
        QPDFEmbeddedFileDocumentHelper efdh(pdf);
        for (auto const& key: m->attachments_to_remove) {
            if (efdh.removeEmbeddedFile(key)) {
                doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                    cout << prefix << ": removed attachment " << key
                         << std::endl;
                });
            } else {
                throw std::runtime_error("attachment " + key + " not found");
            }
        }
    }
    if (!m->attachments_to_add.empty()) {
        addAttachments(pdf);
    }
    if (!m->attachments_to_copy.empty()) {
        copyAttachments(pdf);
    }
}

bool
QPDFJob::shouldRemoveUnreferencedResources(QPDF& pdf)
{
    if (m->remove_unreferenced_page_resources == QPDFJob::re_no) {
        return false;
    } else if (m->remove_unreferenced_page_resources == QPDFJob::re_yes) {
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

    std::set<QPDFObjGen> resources_seen; // shared resources detection
    std::set<QPDFObjGen> nodes_seen;     // loop detection

    doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": " << pdf.getFilename()
             << ": checking for shared resources" << std::endl;
    });

    std::list<QPDFObjectHandle> queue;
    queue.push_back(pdf.getRoot().getKey("/Pages"));
    while (!queue.empty()) {
        QPDFObjectHandle node = *queue.begin();
        queue.pop_front();
        QPDFObjGen og = node.getObjGen();
        if (nodes_seen.count(og)) {
            continue;
        }
        nodes_seen.insert(og);
        QPDFObjectHandle dict = node.isStream() ? node.getDict() : node;
        QPDFObjectHandle kids = dict.getKey("/Kids");
        if (kids.isArray()) {
            // This is a non-leaf node.
            if (dict.hasKey("/Resources")) {
                QTC::TC("qpdf", "QPDFJob found resources in non-leaf");
                doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                    cout << "  found resources in non-leaf page node "
                         << og.getObj() << " " << og.getGen() << std::endl;
                });
                return true;
            }
            int n = kids.getArrayNItems();
            for (int i = 0; i < n; ++i) {
                queue.push_back(kids.getArrayItem(i));
            }
        } else {
            // This is a leaf node or a form XObject.
            QPDFObjectHandle resources = dict.getKey("/Resources");
            if (resources.isIndirect()) {
                QPDFObjGen resources_og = resources.getObjGen();
                if (resources_seen.count(resources_og)) {
                    QTC::TC("qpdf", "QPDFJob found shared resources in leaf");
                    doIfVerbose(
                        [&](std::ostream& cout, std::string const& prefix) {
                            cout << "  found shared resources in leaf node "
                                 << og.getObj() << " " << og.getGen() << ": "
                                 << resources_og.getObj() << " "
                                 << resources_og.getGen() << std::endl;
                        });
                    return true;
                }
                resources_seen.insert(resources_og);
            }
            QPDFObjectHandle xobject =
                (resources.isDictionary() ? resources.getKey("/XObject")
                                          : QPDFObjectHandle::newNull());
            if (xobject.isIndirect()) {
                QPDFObjGen xobject_og = xobject.getObjGen();
                if (resources_seen.count(xobject_og)) {
                    QTC::TC("qpdf", "QPDFJob found shared xobject in leaf");
                    doIfVerbose(
                        [&](std::ostream& cout, std::string const& prefix) {
                            cout << "  found shared xobject in leaf node "
                                 << og.getObj() << " " << og.getGen() << ": "
                                 << xobject_og.getObj() << " "
                                 << xobject_og.getGen() << std::endl;
                        });
                    return true;
                }
                resources_seen.insert(xobject_og);
            }
            if (xobject.isDictionary()) {
                for (auto const& k: xobject.getKeys()) {
                    QPDFObjectHandle xobj = xobject.getKey(k);
                    if (xobj.isFormXObject()) {
                        queue.push_back(xobj);
                    }
                }
            }
        }
    }

    doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": no shared resources found" << std::endl;
    });
    return false;
}

static QPDFObjectHandle
added_page(QPDF& pdf, QPDFObjectHandle page)
{
    QPDFObjectHandle result = page;
    if (page.getOwningQPDF() != &pdf) {
        // Calling copyForeignObject on an object we already copied
        // will give us the already existing copy.
        result = pdf.copyForeignObject(page);
    }
    return result;
}

static QPDFObjectHandle
added_page(QPDF& pdf, QPDFPageObjectHelper page)
{
    return added_page(pdf, page.getObjectHandle());
}

void
QPDFJob::handlePageSpecs(
    QPDF& pdf, bool& warnings, std::vector<std::shared_ptr<QPDF>>& page_heap)
{
    // Parse all page specifications and translate them into lists of
    // actual pages.

    // Handle "." as a shortcut for the input file
    for (auto& page_spec: m->page_specs) {
        if (page_spec.filename == ".") {
            page_spec.filename = m->infilename.get();
        }
    }

    if (!m->keep_files_open_set) {
        // Count the number of distinct files to determine whether we
        // should keep files open or not. Rather than trying to code
        // some portable heuristic based on OS limits, just hard-code
        // this at a given number and allow users to override.
        std::set<std::string> filenames;
        for (auto& page_spec: m->page_specs) {
            filenames.insert(page_spec.filename);
        }
        m->keep_files_open = (filenames.size() <= m->keep_files_open_threshold);
        QTC::TC(
            "qpdf",
            "QPDFJob automatically set keep files open",
            m->keep_files_open ? 0 : 1);
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": selecting --keep-open-files="
                 << (m->keep_files_open ? "y" : "n") << std::endl;
        });
    }

    // Create a QPDF object for each file that we may take pages from.
    std::map<std::string, QPDF*> page_spec_qpdfs;
    std::map<std::string, ClosedFileInputSource*> page_spec_cfis;
    page_spec_qpdfs[m->infilename.get()] = &pdf;
    std::vector<QPDFPageData> parsed_specs;
    std::map<unsigned long long, std::set<QPDFObjGen>> copied_pages;
    for (auto& page_spec: m->page_specs) {
        if (page_spec_qpdfs.count(page_spec.filename) == 0) {
            // Open the PDF file and store the QPDF object. Throw a
            // std::shared_ptr to the qpdf into a heap so that it
            // survives through copying to the output but gets cleaned up
            // automatically at the end. Do not canonicalize the file
            // name. Using two different paths to refer to the same
            // file is a documented workaround for duplicating a page.
            // If you are using this an example of how to do this with
            // the API, you can just create two different QPDF objects
            // to the same underlying file with the same path to
            // achieve the same affect.
            char const* password = page_spec.password.get();
            if ((!m->encryption_file.empty()) && (password == 0) &&
                (page_spec.filename == m->encryption_file)) {
                QTC::TC("qpdf", "QPDFJob pages encryption password");
                password = m->encryption_file_password.get();
            }
            doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
                cout << prefix << ": processing " << page_spec.filename
                     << std::endl;
            });
            std::shared_ptr<InputSource> is;
            ClosedFileInputSource* cis = 0;
            if (!m->keep_files_open) {
                QTC::TC("qpdf", "QPDFJob keep files open n");
                cis = new ClosedFileInputSource(page_spec.filename.c_str());
                is = std::shared_ptr<InputSource>(cis);
                cis->stayOpen(true);
            } else {
                QTC::TC("qpdf", "QPDFJob keep files open y");
                FileInputSource* fis =
                    new FileInputSource(page_spec.filename.c_str());
                is = std::shared_ptr<InputSource>(fis);
            }
            std::shared_ptr<QPDF> qpdf_ph =
                processInputSource(is, password, true);
            page_heap.push_back(qpdf_ph);
            page_spec_qpdfs[page_spec.filename] = qpdf_ph.get();
            if (cis) {
                cis->stayOpen(false);
                page_spec_cfis[page_spec.filename] = cis;
            }
        }

        // Read original pages from the PDF, and parse the page range
        // associated with this occurrence of the file.
        parsed_specs.push_back(
            // line-break
            QPDFPageData(
                page_spec.filename,
                page_spec_qpdfs[page_spec.filename],
                page_spec.range));
    }

    std::map<unsigned long long, bool> remove_unreferenced;
    if (m->remove_unreferenced_page_resources != QPDFJob::re_no) {
        for (auto const& iter: page_spec_qpdfs) {
            std::string const& filename = iter.first;
            ClosedFileInputSource* cis = 0;
            if (page_spec_cfis.count(filename)) {
                cis = page_spec_cfis[filename];
                cis->stayOpen(true);
            }
            QPDF& other(*(iter.second));
            auto other_uuid = other.getUniqueId();
            if (remove_unreferenced.count(other_uuid) == 0) {
                remove_unreferenced[other_uuid] =
                    shouldRemoveUnreferencedResources(other);
            }
            if (cis) {
                cis->stayOpen(false);
            }
        }
    }

    // Clear all pages out of the primary QPDF's pages tree but leave
    // the objects in place in the file so they can be re-added
    // without changing their object numbers. This enables other
    // things in the original file, such as outlines, to continue to
    // work.
    doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
        cout << prefix << ": removing unreferenced pages from primary input"
             << std::endl;
    });
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> orig_pages = dh.getAllPages();
    for (auto const& page: orig_pages) {
        dh.removePage(page);
    }

    if (m->collate && (parsed_specs.size() > 1)) {
        // Collate the pages by selecting one page from each spec in
        // order. When a spec runs out of pages, stop selecting from
        // it.
        std::vector<QPDFPageData> new_parsed_specs;
        size_t nspecs = parsed_specs.size();
        size_t cur_page = 0;
        bool got_pages = true;
        while (got_pages) {
            got_pages = false;
            for (size_t i = 0; i < nspecs; ++i) {
                QPDFPageData& page_data = parsed_specs.at(i);
                for (size_t j = 0; j < m->collate; ++j) {
                    if (cur_page + j < page_data.selected_pages.size()) {
                        got_pages = true;
                        new_parsed_specs.push_back(
                            // line-break
                            QPDFPageData(
                                page_data,
                                page_data.selected_pages.at(cur_page + j)));
                    }
                }
            }
            cur_page += m->collate;
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
    std::map<unsigned long long, std::shared_ptr<QPDFAcroFormDocumentHelper>>
        afdh_map;
    auto this_afdh = get_afdh_for_qpdf(afdh_map, &pdf);
    std::set<QPDFObjGen> referenced_fields;
    for (auto& page_data: parsed_specs) {
        ClosedFileInputSource* cis = 0;
        if (page_spec_cfis.count(page_data.filename)) {
            cis = page_spec_cfis[page_data.filename];
            cis->stayOpen(true);
        }
        QPDFPageLabelDocumentHelper pldh(*page_data.qpdf);
        auto other_afdh = get_afdh_for_qpdf(afdh_map, page_data.qpdf);
        if (pldh.hasPageLabels()) {
            any_page_labels = true;
        }
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": adding pages from " << page_data.filename
                 << std::endl;
        });
        for (auto pageno_iter: page_data.selected_pages) {
            // Pages are specified from 1 but numbered from 0 in the
            // vector
            int pageno = pageno_iter - 1;
            pldh.getLabelsForPageRange(
                pageno, pageno, out_pageno++, new_labels);
            QPDFPageObjectHelper to_copy =
                page_data.orig_pages.at(QIntC::to_size(pageno));
            QPDFObjGen to_copy_og = to_copy.getObjectHandle().getObjGen();
            unsigned long long from_uuid = page_data.qpdf->getUniqueId();
            if (copied_pages[from_uuid].count(to_copy_og)) {
                QTC::TC(
                    "qpdf",
                    "QPDFJob copy same page more than once",
                    (page_data.qpdf == &pdf) ? 0 : 1);
                to_copy = to_copy.shallowCopyPage();
            } else {
                copied_pages[from_uuid].insert(to_copy_og);
                if (remove_unreferenced[from_uuid]) {
                    to_copy.removeUnreferencedResources();
                }
            }
            dh.addPage(to_copy, false);
            bool first_copy_from_orig = false;
            bool this_file = (page_data.qpdf == &pdf);
            if (this_file) {
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
                ((!this_file) || (!first_copy_from_orig))) {
                if (!this_file) {
                    QTC::TC("qpdf", "QPDFJob copy fields not this file");
                } else if (!first_copy_from_orig) {
                    QTC::TC("qpdf", "QPDFJob copy fields non-first from orig");
                }
                try {
                    this_afdh->fixCopiedAnnotations(
                        new_page,
                        to_copy.getObjectHandle(),
                        *other_afdh,
                        &referenced_fields);
                } catch (std::exception& e) {
                    pdf.warn(
                        qpdf_e_damaged_pdf,
                        "",
                        0,
                        ("Exception caught while fixing copied"
                         " annotations. This may be a qpdf bug. " +
                         std::string("Exception: ") + e.what()));
                }
            }
        }
        if (page_data.qpdf->anyWarnings()) {
            warnings = true;
        }
        if (cis) {
            cis->stayOpen(false);
        }
    }
    if (any_page_labels) {
        QPDFObjectHandle page_labels = QPDFObjectHandle::newDictionary();
        page_labels.replaceKey("/Nums", QPDFObjectHandle::newArray(new_labels));
        pdf.getRoot().replaceKey("/PageLabels", page_labels);
    }

    // Delete page objects for unused page in primary. This prevents
    // those objects from being preserved by being referred to from
    // other places, such as the outlines dictionary. Also make sure
    // we keep form fields from pages we preserved.
    for (size_t pageno = 0; pageno < orig_pages.size(); ++pageno) {
        auto page = orig_pages.at(pageno);
        if (selected_from_orig.count(QIntC::to_int(pageno))) {
            for (auto field: this_afdh->getFormFieldsForPage(page)) {
                QTC::TC("qpdf", "QPDFJob pages keeping field from original");
                referenced_fields.insert(field.getObjectHandle().getObjGen());
            }
        } else {
            pdf.replaceObject(
                page.getObjectHandle().getObjGen(),
                QPDFObjectHandle::newNull());
        }
    }
    // Remove unreferenced form fields
    if (this_afdh->hasAcroForm()) {
        auto acroform = pdf.getRoot().getKey("/AcroForm");
        auto fields = acroform.getKey("/Fields");
        if (fields.isArray()) {
            auto new_fields = QPDFObjectHandle::newArray();
            if (fields.isIndirect()) {
                new_fields = pdf.makeIndirectObject(new_fields);
            }
            for (auto const& field: fields.aitems()) {
                if (referenced_fields.count(field.getObjGen())) {
                    new_fields.appendItem(field);
                }
            }
            if (new_fields.getArrayNItems() > 0) {
                QTC::TC("qpdf", "QPDFJob keep some fields in pages");
                acroform.replaceKey("/Fields", new_fields);
            } else {
                QTC::TC("qpdf", "QPDFJob no more fields in pages");
                pdf.getRoot().removeKey("/AcroForm");
            }
        }
    }
}

void
QPDFJob::handleRotations(QPDF& pdf)
{
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    int npages = QIntC::to_int(pages.size());
    for (auto const& iter: m->rotations) {
        std::string const& range = iter.first;
        QPDFJob::RotationSpec const& rspec = iter.second;
        // range has been previously validated
        std::vector<int> to_rotate =
            QUtil::parse_numrange(range.c_str(), npages);
        for (int pageno_iter: to_rotate) {
            int pageno = pageno_iter - 1;
            if ((pageno >= 0) && (pageno < npages)) {
                pages.at(QIntC::to_size(pageno))
                    .rotatePage(rspec.angle, rspec.relative);
            }
        }
    }
}

void
QPDFJob::maybeFixWritePassword(int R, std::string& password)
{
    switch (m->password_mode) {
    case QPDFJob::pm_bytes:
        QTC::TC("qpdf", "QPDFJob password mode bytes");
        break;

    case QPDFJob::pm_hex_bytes:
        QTC::TC("qpdf", "QPDFJob password mode hex-bytes");
        password = QUtil::hex_decode(password);
        break;

    case QPDFJob::pm_unicode:
    case QPDFJob::pm_auto:
        {
            bool has_8bit_chars;
            bool is_valid_utf8;
            bool is_utf16;
            QUtil::analyze_encoding(
                password, has_8bit_chars, is_valid_utf8, is_utf16);
            if (!has_8bit_chars) {
                return;
            }
            if (m->password_mode == QPDFJob::pm_unicode) {
                if (!is_valid_utf8) {
                    QTC::TC("qpdf", "QPDFJob password not unicode");
                    throw std::runtime_error(
                        "supplied password is not valid UTF-8");
                }
                if (R < 5) {
                    std::string encoded;
                    if (!QUtil::utf8_to_pdf_doc(password, encoded)) {
                        QTC::TC("qpdf", "QPDFJob password not encodable");
                        throw std::runtime_error(
                            "supplied password cannot be encoded for"
                            " 40-bit or 128-bit encryption formats");
                    }
                    password = encoded;
                }
            } else {
                if ((R < 5) && is_valid_utf8) {
                    std::string encoded;
                    if (QUtil::utf8_to_pdf_doc(password, encoded)) {
                        QTC::TC("qpdf", "QPDFJob auto-encode password");
                        doIfVerbose(
                            [&](std::ostream& cout, std::string const& prefix) {
                                cout << prefix
                                     << ": automatically converting Unicode"
                                     << " password to single-byte encoding as"
                                     << " required for 40-bit or 128-bit"
                                     << " encryption" << std::endl;
                            });
                        password = encoded;
                    } else {
                        QTC::TC("qpdf", "QPDFJob bytes fallback warning");
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
                } else if ((R >= 5) && (!is_valid_utf8)) {
                    QTC::TC("qpdf", "QPDFJob invalid utf-8 in auto");
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
    int R = 0;
    if (m->keylen == 40) {
        R = 2;
    } else if (m->keylen == 128) {
        if (m->force_V4 || m->cleartext_metadata || m->use_aes) {
            R = 4;
        } else {
            R = 3;
        }
    } else if (m->keylen == 256) {
        if (m->force_R5) {
            R = 5;
        } else {
            R = 6;
        }
    } else {
        throw std::logic_error("bad encryption keylen");
    }
    if ((R > 3) && (m->r3_accessibility == false)) {
        *(this->m->cerr) << this->m->message_prefix
                         << ": -accessibility=n is ignored for modern"
                         << " encryption formats" << std::endl;
    }
    maybeFixWritePassword(R, m->user_password);
    maybeFixWritePassword(R, m->owner_password);
    if ((R < 4) || ((R == 4) && (!m->use_aes))) {
        if (!m->allow_weak_crypto) {
            QTC::TC("qpdf", "QPDFJob weak crypto error");
            *(this->m->cerr)
                << this->m->message_prefix
                << ": refusing to write a file with RC4, a weak cryptographic "
                   "algorithm"
                << std::endl
                << "Please use 256-bit keys for better security." << std::endl
                << "Pass --allow-weak-crypto to enable writing insecure files."
                << std::endl
                << "See also "
                   "https://qpdf.readthedocs.io/en/stable/weak-crypto.html"
                << std::endl;
            throw std::runtime_error(
                "refusing to write a file with weak crypto");
        }
    }
    switch (R) {
    case 2:
        w.setR2EncryptionParametersInsecure(
            m->user_password.c_str(),
            m->owner_password.c_str(),
            m->r2_print,
            m->r2_modify,
            m->r2_extract,
            m->r2_annotate);
        break;
    case 3:
        w.setR3EncryptionParametersInsecure(
            m->user_password.c_str(),
            m->owner_password.c_str(),
            m->r3_accessibility,
            m->r3_extract,
            m->r3_assemble,
            m->r3_annotate_and_form,
            m->r3_form_filling,
            m->r3_modify_other,
            m->r3_print);
        break;
    case 4:
        w.setR4EncryptionParametersInsecure(
            m->user_password.c_str(),
            m->owner_password.c_str(),
            m->r3_accessibility,
            m->r3_extract,
            m->r3_assemble,
            m->r3_annotate_and_form,
            m->r3_form_filling,
            m->r3_modify_other,
            m->r3_print,
            !m->cleartext_metadata,
            m->use_aes);
        break;
    case 5:
        w.setR5EncryptionParameters(
            m->user_password.c_str(),
            m->owner_password.c_str(),
            m->r3_accessibility,
            m->r3_extract,
            m->r3_assemble,
            m->r3_annotate_and_form,
            m->r3_form_filling,
            m->r3_modify_other,
            m->r3_print,
            !m->cleartext_metadata);
        break;
    case 6:
        w.setR6EncryptionParameters(
            m->user_password.c_str(),
            m->owner_password.c_str(),
            m->r3_accessibility,
            m->r3_extract,
            m->r3_assemble,
            m->r3_annotate_and_form,
            m->r3_form_filling,
            m->r3_modify_other,
            m->r3_print,
            !m->cleartext_metadata);
        break;
    default:
        throw std::logic_error("bad encryption R value");
        break;
    }
}

static void
parse_version(
    std::string const& full_version_string,
    std::string& version,
    int& extension_level)
{
    auto vp = QUtil::make_unique_cstr(full_version_string);
    char* v = vp.get();
    char* p1 = strchr(v, '.');
    char* p2 = (p1 ? strchr(1 + p1, '.') : 0);
    if (p2 && *(p2 + 1)) {
        *p2++ = '\0';
        extension_level = QUtil::string_to_int(p2);
    }
    version = v;
}

void
QPDFJob::setWriterOptions(QPDF& pdf, QPDFWriter& w)
{
    if (m->compression_level >= 0) {
        Pl_Flate::setCompressionLevel(m->compression_level);
    }
    if (m->qdf_mode) {
        w.setQDFMode(true);
    }
    if (m->preserve_unreferenced_objects) {
        w.setPreserveUnreferencedObjects(true);
    }
    if (m->newline_before_endstream) {
        w.setNewlineBeforeEndstream(true);
    }
    if (m->normalize_set) {
        w.setContentNormalization(m->normalize);
    }
    if (m->stream_data_set) {
        w.setStreamDataMode(m->stream_data_mode);
    }
    if (m->compress_streams_set) {
        w.setCompressStreams(m->compress_streams);
    }
    if (m->recompress_flate_set) {
        w.setRecompressFlate(m->recompress_flate);
    }
    if (m->decode_level_set) {
        w.setDecodeLevel(m->decode_level);
    }
    if (m->decrypt) {
        w.setPreserveEncryption(false);
    }
    if (m->deterministic_id) {
        w.setDeterministicID(true);
    }
    if (m->static_id) {
        w.setStaticID(true);
    }
    if (m->static_aes_iv) {
        w.setStaticAesIV(true);
    }
    if (m->suppress_original_object_id) {
        w.setSuppressOriginalObjectIDs(true);
    }
    if (m->copy_encryption) {
        std::shared_ptr<QPDF> encryption_pdf = processFile(
            m->encryption_file.c_str(),
            m->encryption_file_password.get(),
            false);
        w.copyEncryptionParameters(*encryption_pdf);
    }
    if (m->encrypt) {
        setEncryptionOptions(pdf, w);
    }
    if (m->linearize) {
        w.setLinearization(true);
    }
    if (!m->linearize_pass1.empty()) {
        w.setLinearizationPass1Filename(m->linearize_pass1);
    }
    if (m->object_stream_set) {
        w.setObjectStreamMode(m->object_stream_mode);
    }
    w.setMinimumPDFVersion(this->m->max_input_version);
    if (!m->min_version.empty()) {
        std::string version;
        int extension_level = 0;
        parse_version(m->min_version, version, extension_level);
        w.setMinimumPDFVersion(version, extension_level);
    }
    if (!m->force_version.empty()) {
        std::string version;
        int extension_level = 0;
        parse_version(m->force_version, version, extension_level);
        w.forcePDFVersion(version, extension_level);
    }
    if (m->progress && m->outfilename) {
        w.registerProgressReporter(
            std::shared_ptr<QPDFWriter::ProgressReporter>(
                // line-break
                new ProgressReporter(
                    *(this->m->cout),
                    this->m->message_prefix,
                    m->outfilename.get())));
    }
}

void
QPDFJob::doSplitPages(QPDF& pdf, bool& warnings)
{
    // Generate output file pattern
    std::string before;
    std::string after;
    size_t len = strlen(m->outfilename.get());
    char* num_spot = strstr(const_cast<char*>(m->outfilename.get()), "%d");
    if (num_spot != 0) {
        QTC::TC("qpdf", "QPDFJob split-pages %d");
        before = std::string(
            m->outfilename.get(),
            QIntC::to_size(num_spot - m->outfilename.get()));
        after = num_spot + 2;
    } else if (
        (len >= 4) &&
        (QUtil::str_compare_nocase(m->outfilename.get() + len - 4, ".pdf") ==
         0)) {
        QTC::TC("qpdf", "QPDFJob split-pages .pdf");
        before = std::string(m->outfilename.get(), len - 4) + "-";
        after = m->outfilename.get() + len - 4;
    } else {
        QTC::TC("qpdf", "QPDFJob split-pages other");
        before = std::string(m->outfilename.get()) + "-";
    }

    if (shouldRemoveUnreferencedResources(pdf)) {
        QPDFPageDocumentHelper dh(pdf);
        dh.removeUnreferencedResources();
    }
    QPDFPageLabelDocumentHelper pldh(pdf);
    QPDFAcroFormDocumentHelper afdh(pdf);
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
    size_t pageno_len = QUtil::uint_to_string(pages.size()).length();
    size_t num_pages = pages.size();
    for (size_t i = 0; i < num_pages; i += QIntC::to_size(m->split_pages)) {
        size_t first = i + 1;
        size_t last = i + QIntC::to_size(m->split_pages);
        if (last > num_pages) {
            last = num_pages;
        }
        QPDF outpdf;
        outpdf.emptyPDF();
        std::shared_ptr<QPDFAcroFormDocumentHelper> out_afdh;
        if (afdh.hasAcroForm()) {
            out_afdh = std::make_shared<QPDFAcroFormDocumentHelper>(outpdf);
        }
        if (m->suppress_warnings) {
            outpdf.setSuppressWarnings(true);
        }
        for (size_t pageno = first; pageno <= last; ++pageno) {
            QPDFObjectHandle page = pages.at(pageno - 1);
            outpdf.addPage(page, false);
            auto new_page = added_page(outpdf, page);
            if (out_afdh.get()) {
                QTC::TC("qpdf", "QPDFJob copy form fields in split_pages");
                try {
                    out_afdh->fixCopiedAnnotations(new_page, page, afdh);
                } catch (std::exception& e) {
                    pdf.warn(
                        qpdf_e_damaged_pdf,
                        "",
                        0,
                        ("Exception caught while fixing copied"
                         " annotations. This may be a qpdf bug." +
                         std::string("Exception: ") + e.what()));
                }
            }
        }
        if (pldh.hasPageLabels()) {
            std::vector<QPDFObjectHandle> labels;
            pldh.getLabelsForPageRange(
                QIntC::to_longlong(first - 1),
                QIntC::to_longlong(last - 1),
                0,
                labels);
            QPDFObjectHandle page_labels = QPDFObjectHandle::newDictionary();
            page_labels.replaceKey("/Nums", QPDFObjectHandle::newArray(labels));
            outpdf.getRoot().replaceKey("/PageLabels", page_labels);
        }
        std::string page_range =
            QUtil::uint_to_string(first, QIntC::to_int(pageno_len));
        if (m->split_pages > 1) {
            page_range +=
                "-" + QUtil::uint_to_string(last, QIntC::to_int(pageno_len));
        }
        std::string outfile = before + page_range + after;
        if (QUtil::same_file(m->infilename.get(), outfile.c_str())) {
            throw std::runtime_error(
                "split pages would overwrite input file with " + outfile);
        }
        QPDFWriter w(outpdf, outfile.c_str());
        setWriterOptions(outpdf, w);
        w.write();
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": wrote file " << outfile << std::endl;
        });
        if (outpdf.anyWarnings()) {
            warnings = true;
        }
    }
}

void
QPDFJob::writeOutfile(QPDF& pdf)
{
    std::shared_ptr<char> temp_out;
    if (m->replace_input) {
        // Append but don't prepend to the path to generate a
        // temporary name. This saves us from having to split the path
        // by directory and non-directory.
        temp_out = QUtil::make_shared_cstr(
            std::string(m->infilename.get()) + ".~qpdf-temp#");
        // m->outfilename will be restored to 0 before temp_out
        // goes out of scope.
        m->outfilename = temp_out;
    } else if (strcmp(m->outfilename.get(), "-") == 0) {
        m->outfilename = 0;
    }
    {
        // Private scope so QPDFWriter will close the output file
        QPDFWriter w(pdf, m->outfilename.get());
        setWriterOptions(pdf, w);
        w.write();
    }
    if (m->outfilename) {
        doIfVerbose([&](std::ostream& cout, std::string const& prefix) {
            cout << prefix << ": wrote file " << m->outfilename << std::endl;
        });
    }
    if (m->replace_input) {
        m->outfilename = 0;
    }
    if (m->replace_input) {
        // We must close the input before we can rename files
        pdf.closeInputSource();
        std::string backup = std::string(m->infilename.get()) + ".~qpdf-orig";
        bool warnings = pdf.anyWarnings();
        if (!warnings) {
            backup.append(1, '#');
        }
        QUtil::rename_file(m->infilename.get(), backup.c_str());
        QUtil::rename_file(temp_out.get(), m->infilename.get());
        if (warnings) {
            *(this->m->cerr) << this->m->message_prefix
                             << ": there are warnings; original file kept in "
                             << backup << std::endl;
        } else {
            try {
                QUtil::remove_file(backup.c_str());
            } catch (QPDFSystemError& e) {
                *(this->m->cerr)
                    << this->m->message_prefix
                    << ": unable to delete original file (" << e.what() << ");"
                    << " original file left in " << backup
                    << ", but the input was successfully replaced" << std::endl;
            }
        }
    }
}

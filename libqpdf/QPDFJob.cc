#include <qpdf/QPDFJob.hh>

#include <cstring>
#include <iostream>
#include <memory>

#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

#include <qpdf/auto_job_schema.hh> // JOB_SCHEMA_DATA

using namespace qpdf;

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
            int quality,
            QPDFObjectHandle& image);
        ~ImageOptimizer() override = default;
        void provideStreamData(QPDFObjGen const&, Pipeline* pipeline) override;
        std::shared_ptr<Pipeline> makePipeline(std::string const& description, Pipeline* next);
        bool evaluate(std::string const& description);

      private:
        QPDFJob& o;
        size_t oi_min_width;
        size_t oi_min_height;
        size_t oi_min_area;
        qpdf_stream_decode_level_e decode_level{qpdf_dl_specialized};
        QPDFObjectHandle image;
        std::shared_ptr<Pl_DCT::CompressConfig> config;
    };

    class DiscardContents: public QPDFObjectHandle::ParserCallbacks
    {
      public:
        ~DiscardContents() override = default;
        void
        handleObject(QPDFObjectHandle) override
        {
        }
        void
        handleEOF() override
        {
        }
    };

    struct QPDFPageData
    {
        QPDFPageData(std::string const& filename, QPDF* qpdf, std::string const& range);
        QPDFPageData(QPDFPageData const& other, int page);

        std::string filename;
        QPDF* qpdf;
        std::vector<QPDFObjectHandle> orig_pages;
        std::vector<int> selected_pages;
    };

    class ProgressReporter: public QPDFWriter::ProgressReporter
    {
      public:
        ProgressReporter(Pipeline& p, std::string const& prefix, char const* filename) :
            p(p),
            prefix(prefix),
            filename(filename)
        {
        }
        ~ProgressReporter() override = default;
        void reportProgress(int) override;

      private:
        Pipeline& p;
        std::string prefix;
        std::string filename;
    };
} // namespace

ImageOptimizer::ImageOptimizer(
    QPDFJob& o,
    size_t oi_min_width,
    size_t oi_min_height,
    size_t oi_min_area,
    int quality,
    QPDFObjectHandle& image) :
    o(o),
    oi_min_width(oi_min_width),
    oi_min_height(oi_min_height),
    oi_min_area(oi_min_area),
    image(image)
{
    if (quality >= 0) {
        // Recompress existing jpeg.
        decode_level = qpdf_dl_all;
        config = Pl_DCT::make_compress_config(
            [quality](jpeg_compress_struct* cinfo) { jpeg_set_quality(cinfo, quality, false); });
    }
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
            o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                v << prefix << ": " << description
                  << ": not optimizing because image dictionary is missing required keys\n";
            });
        }
        return result;
    }
    QPDFObjectHandle components_obj = dict.getKey("/BitsPerComponent");
    if (!(components_obj.isInteger() && (components_obj.getIntValue() == 8))) {
        QTC::TC("qpdf", "QPDFJob image optimize bits per component");
        if (!description.empty()) {
            o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                v << prefix << ": " << description
                  << ": not optimizing because image has other than 8 bits per component\n";
            });
        }
        return result;
    }
    // Files have been seen in the wild whose width and height are floating point, which is goofy,
    // but we can deal with it.
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
    std::string colorspace = (colorspace_obj.isName() ? colorspace_obj.getName() : std::string());
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
            o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                v << prefix << ": " << description
                  << ": not optimizing because qpdf can't optimize images with this colorspace\n";
            });
        }
        return result;
    }
    if (((this->oi_min_width > 0) && (w <= this->oi_min_width)) ||
        ((this->oi_min_height > 0) && (h <= this->oi_min_height)) ||
        ((this->oi_min_area > 0) && ((w * h) <= this->oi_min_area))) {
        QTC::TC("qpdf", "QPDFJob image optimize too small");
        if (!description.empty()) {
            o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                v << prefix << ": " << description
                  << ": not optimizing because image is smaller than requested minimum "
                     "dimensions\n";
            });
        }
        return result;
    }

    result = std::make_shared<Pl_DCT>("jpg", next, w, h, components, cs, config.get());
    return result;
}

bool
ImageOptimizer::evaluate(std::string const& description)
{
    // Note: passing nullptr as pipeline (first argument) just tests whether we can filter.
    if (!image.pipeStreamData(nullptr, 0, decode_level, true)) {
        QTC::TC("qpdf", "QPDFJob image optimize no pipeline");
        o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": " << description
              << ": not optimizing because unable to decode data or data already uses DCT\n";
        });
        return false;
    }
    Pl_Discard d;
    Pl_Count c("count", &d);
    std::shared_ptr<Pipeline> p = makePipeline(description, &c);
    if (p == nullptr) {
        // message issued by makePipeline
        return false;
    }
    if (!image.pipeStreamData(p.get(), 0, decode_level)) {
        return false;
    }
    long long orig_length = image.getDict().getKey("/Length").getIntValue();
    if (c.getCount() >= orig_length) {
        QTC::TC("qpdf", "QPDFJob image optimize no shrink");
        o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": " << description
              << ": not optimizing because DCT compression does not reduce image size\n";
        });
        return false;
    }
    o.doIfVerbose([&](Pipeline& v, std::string const& prefix) {
        v << prefix << ": " << description << ": optimizing image reduces size from " << orig_length
          << " to " << c.getCount() << "\n";
    });
    return true;
}

void
ImageOptimizer::provideStreamData(QPDFObjGen const&, Pipeline* pipeline)
{
    std::shared_ptr<Pipeline> p = makePipeline("", pipeline);
    if (p == nullptr) {
        // Should not be possible
        image.warnIfPossible(
            "unable to create pipeline after previous success; image data will be lost");
        pipeline->finish();
        return;
    }
    image.pipeStreamData(p.get(), 0, decode_level, false, false);
}

QPDFJob::PageSpec::PageSpec(
    std::string const& filename, char const* password, std::string const& range) :
    filename(filename),
    range(range)
{
    if (password) {
        this->password = QUtil::make_shared_cstr(password);
    }
}

QPDFPageData::QPDFPageData(std::string const& filename, QPDF* qpdf, std::string const& range) :
    filename(filename),
    qpdf(qpdf),
    orig_pages(qpdf->getAllPages())
{
    try {
        this->selected_pages =
            QUtil::parse_numrange(range.c_str(), QIntC::to_int(this->orig_pages.size()));
    } catch (std::runtime_error& e) {
        throw std::runtime_error("parsing numeric range for " + filename + ": " + e.what());
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
    this->p << prefix << ": " << filename << ": write progress: " << percentage << "%\n";
}

QPDFJob::Members::Members() :
    log(QPDFLogger::defaultLogger())
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
    m->message_prefix = message_prefix;
}

std::string
QPDFJob::getMessagePrefix() const
{
    return m->message_prefix;
}

std::shared_ptr<QPDFLogger>
QPDFJob::getLogger()
{
    return m->log;
}

void
QPDFJob::setLogger(std::shared_ptr<QPDFLogger> l)
{
    m->log = l;
}

void
QPDFJob::setOutputStreams(std::ostream* out, std::ostream* err)
{
    setLogger(QPDFLogger::create());
    m->log->setOutputStreams(out, err);
}

void
QPDFJob::registerProgressReporter(std::function<void(int)> handler)
{
    m->progress_handler = handler;
}

void
QPDFJob::doIfVerbose(std::function<void(Pipeline&, std::string const& prefix)> fn)
{
    if (m->verbose) {
        fn(*m->log->getInfo(), m->message_prefix);
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
    return job_json_schema(1);
}

std::string
QPDFJob::job_json_schema(int version)
{
    if (version != LATEST_JOB_JSON) {
        throw std::runtime_error("job_json_schema: version must be 1");
    }
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
        } else if (!util::is_digit(angle_str.at(0))) {
            angle_str = "";
        }
    }
    if (range.empty()) {
        range = "1-z";
    }
    try {
        QUtil::parse_numrange(range.c_str(), 0);
    } catch (std::runtime_error const&) {
        usage("invalid parameter to rotate: " + parameter);
    }
    int angle = QUtil::string_to_int(angle_str.c_str()) % 360;
    if (angle % 90 != 0) {
        usage("invalid parameter to rotate (angle must be a multiple of 90): " + parameter);
    }
    if (relative == -1) {
        angle = -angle;
    }
    m->rotations[range] = RotationSpec(angle, (relative != 0));
}

std::vector<int>
QPDFJob::parseNumrange(char const* range, int max)
{
    try {
        return QUtil::parse_numrange(range, max);
    } catch (std::runtime_error& e) {
        usage(e.what());
    }
    return {};
}

std::unique_ptr<QPDF>
QPDFJob::createQPDF()
{
    checkConfiguration();
    std::unique_ptr<QPDF> pdf_sp;
    try {
        processFile(pdf_sp, m->infilename.get(), m->password.get(), true, true);
    } catch (QPDFExc& e) {
        if (e.getErrorCode() == qpdf_e_password) {
            // Allow certain operations to work when an incorrect password is supplied.
            if (m->check_is_encrypted || m->check_requires_password) {
                m->encryption_status = qpdf_es_encrypted | qpdf_es_password_incorrect;
                return nullptr;
            }
            if (m->show_encryption && pdf_sp) {
                m->log->info("Incorrect password supplied\n");
                showEncryption(*pdf_sp);
                return nullptr;
            }
        }
        throw;
    }
    QPDF& pdf = *pdf_sp;
    if (pdf.isEncrypted()) {
        m->encryption_status = qpdf_es_encrypted;
    }

    if (m->check_is_encrypted || m->check_requires_password) {
        return nullptr;
    }

    // If we are updating from JSON, this has to be done first before other options may cause
    // transformations to the input.
    if (!m->update_from_json.empty()) {
        pdf.updateFromJSON(m->update_from_json);
    }

    std::vector<std::unique_ptr<QPDF>> page_heap;
    if (!m->page_specs.empty()) {
        handlePageSpecs(pdf, page_heap);
    }
    if (!m->rotations.empty()) {
        handleRotations(pdf);
    }
    handleUnderOverlay(pdf);
    handleTransformations(pdf);
    if (m->remove_info) {
        auto trailer = pdf.getTrailer();
        auto mod_date = trailer.getKey("/Info").getKeyIfDict("/ModDate");
        if (mod_date.isNull()) {
            trailer.removeKey("/Info");
        } else {
            auto info = trailer.replaceKeyAndGetNew(
                "/Info", pdf.makeIndirectObject(QPDFObjectHandle::newDictionary()));
            info.replaceKey("/ModDate", mod_date);
        }
        pdf.getRoot().removeKey("/Metadata");
    }
    if (m->remove_metadata) {
        pdf.getRoot().removeKey("/Metadata");
    }
    if (m->remove_structure) {
        pdf.getRoot().removeKey("/StructTreeRoot");
        pdf.getRoot().removeKey("/MarkInfo");
    }

    for (auto& foreign: page_heap) {
        if (foreign->anyWarnings()) {
            m->warnings = true;
        }
    }
    return pdf_sp;
}

void
QPDFJob::writeQPDF(QPDF& pdf)
{
    if (createsOutput()) {
        if (!Pl_Flate::zopfli_check_env(pdf.getLogger().get())) {
            m->warnings = true;
        }
    }
    if (!createsOutput()) {
        doInspection(pdf);
    } else if (m->split_pages) {
        doSplitPages(pdf);
    } else {
        writeOutfile(pdf);
    }
    if (!pdf.getWarnings().empty()) {
        m->warnings = true;
    }
    if (m->warnings && (!m->suppress_warnings)) {
        if (createsOutput()) {
            *m->log->getWarn()
                << m->message_prefix
                << ": operation succeeded with warnings; resulting file may have some problems\n";
        } else {
            *m->log->getWarn() << m->message_prefix << ": operation succeeded with warnings\n";
        }
    }
    if (m->report_mem_usage) {
        // Call get_max_memory_usage before generating output. When debugging, it's easier if print
        // statements from get_max_memory_usage are not interleaved with the output.
        auto mem_usage = QUtil::get_max_memory_usage();
        *m->log->getWarn() << "qpdf-max-memory-usage " << mem_usage << "\n";
    }
}

void
QPDFJob::run()
{
    auto pdf = createQPDF();
    if (pdf) {
        writeQPDF(*pdf);
    }
}

bool
QPDFJob::hasWarnings() const
{
    return m->warnings;
}

bool
QPDFJob::createsOutput() const
{
    return ((m->outfilename != nullptr) || m->replace_input);
}

int
QPDFJob::getExitCode() const
{
    if (m->check_is_encrypted) {
        if (m->encryption_status & qpdf_es_encrypted) {
            QTC::TC("qpdf", "QPDFJob check encrypted encrypted");
            return 0;
        } else {
            QTC::TC("qpdf", "QPDFJob check encrypted not encrypted");
            return EXIT_IS_NOT_ENCRYPTED;
        }
    } else if (m->check_requires_password) {
        if (m->encryption_status & qpdf_es_encrypted) {
            if (m->encryption_status & qpdf_es_password_incorrect) {
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

    if (m->warnings && (!m->warnings_exit_zero)) {
        return EXIT_WARNING;
    }
    return 0;
}

void
QPDFJob::checkConfiguration()
{
    // Do final checks for command-line consistency. (I always think this is called doFinalChecks,
    // so I'm putting that in a comment.)

    if (m->replace_input) {
        // Check for --empty appears later after we have checked m->infilename.
        if (m->outfilename) {
            usage("--replace-input may not be used when an output file is specified");
        } else if (m->split_pages) {
            usage("--split-pages may not be used with --replace-input");
        } else if (m->json_version) {
            usage("--json may not be used with --replace-input");
        }
    }
    if (m->json_version && (m->outfilename == nullptr)) {
        // The output file is optional with --json for backward compatibility and defaults to
        // standard output.
        m->outfilename = QUtil::make_shared_cstr("-");
    }
    if (m->infilename == nullptr) {
        usage("an input file name is required");
    } else if (m->replace_input && (strlen(m->infilename.get()) == 0)) {
        usage("--replace-input may not be used with --empty");
    } else if (m->require_outfile && (m->outfilename == nullptr) && (!m->replace_input)) {
        usage("an output file name is required; use - for standard output");
    } else if ((!m->require_outfile) && ((m->outfilename != nullptr) || m->replace_input)) {
        usage("no output file may be given for this option");
    }
    if (m->check_requires_password && m->check_is_encrypted) {
        usage("--requires-password and --is-encrypted may not be given together");
    }

    if (m->encrypt && (!m->allow_insecure) &&
        (m->owner_password.empty() && (!m->user_password.empty()) && (m->keylen == 256))) {
        // Note that empty owner passwords for R < 5 are copied from the user password, so this lack
        // of security is not an issue for those files. Also we are consider only the ability to
        // open the file without a password to be insecure. We are not concerned about whether the
        // viewer enforces security settings when the user and owner password match.
        usage(
            "A PDF with a non-empty user password and an empty owner password encrypted with a "
            "256-bit key is insecure as it can be opened without a password. If you really want to"
            " do this, you must also give the --allow-insecure option before the -- that follows "
            "--encrypt.");
    }

    bool save_to_stdout = false;
    if (m->require_outfile && m->outfilename && (strcmp(m->outfilename.get(), "-") == 0)) {
        if (m->split_pages) {
            usage("--split-pages may not be used when writing to standard output");
        }
        save_to_stdout = true;
    }
    if (!m->attachment_to_show.empty()) {
        save_to_stdout = true;
    }
    if (save_to_stdout) {
        m->log->saveToStandardOutput(true);
    }
    if ((!m->split_pages) && QUtil::same_file(m->infilename.get(), m->outfilename.get())) {
        QTC::TC("qpdf", "QPDFJob same file error");
        usage(
            "input file and output file are the same; use --replace-input to intentionally "
            "overwrite the input file");
    }

    if (m->json_version == 1) {
        if (m->json_keys.count("qpdf")) {
            usage("json key \"qpdf\" is only valid for json version > 1");
        }
    } else {
        if (m->json_keys.count("objectinfo") || m->json_keys.count("objects")) {
            usage("json keys \"objects\" and \"objectinfo\" are only valid for json version 1");
        }
    }
}

unsigned long
QPDFJob::getEncryptionStatus()
{
    return m->encryption_status;
}

void
QPDFJob::setQPDFOptions(QPDF& pdf)
{
    pdf.setLogger(m->log);
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
    auto& cout = *m->log->getInfo();
    if (!pdf.isEncrypted(R, P, V, stream_method, string_method, file_method)) {
        cout << "File is not encrypted\n";
    } else {
        cout << "R = " << R << "\n";
        cout << "P = " << P << "\n";
        std::string user_password = pdf.getTrimmedUserPassword();
        std::string encryption_key = pdf.getEncryptionKey();
        cout << "User password = " << user_password << "\n";
        if (m->show_encryption_key) {
            cout << "Encryption key = " << QUtil::hex_encode(encryption_key) << "\n";
        }
        if (pdf.ownerPasswordMatched()) {
            cout << "Supplied password is owner password\n";
        }
        if (pdf.userPasswordMatched()) {
            cout << "Supplied password is user password\n";
        }
        cout << "extract for accessibility: " << show_bool(pdf.allowAccessibility()) << "\n"
             << "extract for any purpose: " << show_bool(pdf.allowExtractAll()) << "\n"
             << "print low resolution: " << show_bool(pdf.allowPrintLowRes()) << "\n"
             << "print high resolution: " << show_bool(pdf.allowPrintHighRes()) << "\n"
             << "modify document assembly: " << show_bool(pdf.allowModifyAssembly()) << "\n"
             << "modify forms: " << show_bool(pdf.allowModifyForm()) << "\n"
             << "modify annotations: " << show_bool(pdf.allowModifyAnnotation()) << "\n"
             << "modify other: " << show_bool(pdf.allowModifyOther()) << "\n"
             << "modify anything: " << show_bool(pdf.allowModifyAll()) << "\n";
        if (V >= 4) {
            cout << "stream encryption method: " << show_encryption_method(stream_method) << "\n"
                 << "string encryption method: " << show_encryption_method(string_method) << "\n"
                 << "file encryption method: " << show_encryption_method(file_method) << "\n";
        }
    }
}

void
QPDFJob::doCheck(QPDF& pdf)
{
    // Code below may set okay to false but not to true. We assume okay until we prove otherwise but
    // may continue to perform additional checks after finding errors.
    bool okay = true;
    auto& cout = *m->log->getInfo();
    cout << "checking " << m->infilename.get() << "\n";
    QPDF::JobSetter::setCheckMode(pdf, true);
    try {
        int extension_level = pdf.getExtensionLevel();
        cout << "PDF Version: " << pdf.getPDFVersion();
        if (extension_level > 0) {
            cout << " extension level " << pdf.getExtensionLevel();
        }
        cout << "\n";
        showEncryption(pdf);
        if (pdf.isLinearized()) {
            cout << "File is linearized\n";
            pdf.checkLinearization();
        } else {
            cout << "File is not linearized\n";
        }

        // Write the file to nowhere, uncompressing streams.  This causes full file traversal and
        // decoding of all streams we can decode.
        QPDFWriter w(pdf);
        Pl_Discard discard;
        w.setOutputPipeline(&discard);
        w.setDecodeLevel(qpdf_dl_all);
        w.write();

        // Parse all content streams
        DiscardContents discard_contents;
        int pageno = 0;
        for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
            ++pageno;
            try {
                page.parseContents(&discard_contents);
            } catch (QPDFExc& e) {
                okay = false;
                *m->log->getError() << "ERROR: page " << pageno << ": " << e.what() << "\n";
            }
        }
    } catch (std::exception& e) {
        *m->log->getError() << "ERROR: " << e.what() << "\n";
        okay = false;
    }
    if (!okay) {
        throw std::runtime_error("errors detected");
    }

    if (!pdf.getWarnings().empty()) {
        m->warnings = true;
    } else {
        *m->log->getInfo()
            << "No syntax or stream encoding errors found; the file may still contain\n"
            << "errors that qpdf cannot detect\n";
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
            if (filter && (!obj.pipeStreamData(nullptr, 0, qpdf_dl_all))) {
                QTC::TC("qpdf", "QPDFJob unable to filter");
                obj.warnIfPossible("unable to filter stream data");
                error = true;
            } else {
                // If anything has been written to standard output, this will fail.
                m->log->saveToStandardOutput(true);
                obj.pipeStreamData(
                    m->log->getSave().get(),
                    (filter && m->normalize) ? qpdf_ef_normalize : 0,
                    filter ? qpdf_dl_all : qpdf_dl_none);
            }
        } else {
            *m->log->getInfo() << "Object is stream.  Dictionary:\n"
                               << obj.getDict().unparseResolved() << "\n";
        }
    } else {
        *m->log->getInfo() << obj.unparseResolved() << "\n";
    }
    if (error) {
        throw std::runtime_error("unable to get object " + obj.getObjGen().unparse(','));
    }
}

void
QPDFJob::doShowPages(QPDF& pdf)
{
    int pageno = 0;
    auto& cout = *m->log->getInfo();
    for (auto& ph: QPDFPageDocumentHelper(pdf).getAllPages()) {
        QPDFObjectHandle page = ph.getObjectHandle();
        ++pageno;

        cout << "page " << pageno << ": " << page.getObjectID() << " " << page.getGeneration()
             << " R\n";
        if (m->show_page_images) {
            std::map<std::string, QPDFObjectHandle> images = ph.getImages();
            if (!images.empty()) {
                cout << "  images:\n";
                for (auto const& iter2: images) {
                    std::string const& name = iter2.first;
                    QPDFObjectHandle image = iter2.second;
                    QPDFObjectHandle dict = image.getDict();
                    int width = dict.getKey("/Width").getIntValueAsInt();
                    int height = dict.getKey("/Height").getIntValueAsInt();
                    cout << "    " << name << ": " << image.unparse() << ", " << width << " x "
                         << height << "\n";
                }
            }
        }

        cout << "  content:\n";
        for (auto& iter2: ph.getPageContents()) {
            cout << "    " << iter2.unparse() << "\n";
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
            *m->log->getInfo() << key << " -> "
                               << efoh->getEmbeddedFileStream().getObjGen().unparse(',') << "\n";
            doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                auto desc = efoh->getDescription();
                if (!desc.empty()) {
                    v << "  description: " << desc << "\n";
                }
                v << "  preferred name: " << efoh->getFilename() << "\n";
                v << "  all names:\n";
                for (auto const& i2: efoh->getFilenames()) {
                    v << "    " << i2.first << " -> " << i2.second << "\n";
                }
                v << "  all data streams:\n";
                for (auto const& [key2, value2]: efoh->getEmbeddedFileStreams().as_dictionary()) {
                    if (value2.null()) {
                        continue;
                    }
                    auto efs = QPDFEFStreamObjectHelper(value2);
                    v << "    " << key2 << " -> " << efs.getObjectHandle().getObjGen().unparse(',')
                      << "\n";
                    v << "      creation date: " << efs.getCreationDate() << "\n"
                      << "      modification date: " << efs.getModDate() << "\n"
                      << "      mime type: " << efs.getSubtype() << "\n"
                      << "      checksum: " << QUtil::hex_encode(efs.getChecksum()) << "\n";
                }
            });
        }
    } else {
        *m->log->getInfo() << m->infilename.get() << " has no embedded files\n";
    }
}

void
QPDFJob::doShowAttachment(QPDF& pdf)
{
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    auto fs = efdh.getEmbeddedFile(m->attachment_to_show);
    if (!fs) {
        throw std::runtime_error("attachment " + m->attachment_to_show + " not found");
    }
    auto efs = fs->getEmbeddedFileStream();
    // saveToStandardOutput has already been called, but it's harmless to call it again, so do as
    // defensive coding.
    m->log->saveToStandardOutput(true);
    efs.pipeStreamData(m->log->getSave().get(), 0, qpdf_dl_all);
}

void
QPDFJob::parse_object_id(std::string const& objspec, bool& trailer, int& obj, int& gen)
{
    if (objspec == "trailer") {
        trailer = true;
    } else {
        trailer = false;
        obj = QUtil::string_to_int(objspec.c_str());
        size_t comma = objspec.find(',');
        if ((comma != std::string::npos) && (comma + 1 < objspec.length())) {
            gen = QUtil::string_to_int(objspec.substr(1 + comma, std::string::npos).c_str());
        }
    }
}

QPDFObjGen::set
QPDFJob::getWantedJSONObjects()
{
    QPDFObjGen::set wanted_og;
    for (auto const& iter: m->json_objects) {
        bool trailer;
        int obj = 0;
        int gen = 0;
        parse_object_id(iter, trailer, obj, gen);
        wanted_og.add(QPDFObjGen(obj, gen));
    }
    return wanted_og;
}

void
QPDFJob::doJSONObjects(Pipeline* p, bool& first, QPDF& pdf)
{
    if (m->json_version == 1) {
        JSON::writeDictionaryKey(p, first, "objects", 1);
        bool first_object = true;
        JSON::writeDictionaryOpen(p, first_object, 1);
        bool all_objects = m->json_objects.empty();
        auto wanted_og = getWantedJSONObjects();
        for (auto& obj: pdf.getAllObjects()) {
            std::string key = obj.unparse();

            if (all_objects || wanted_og.count(obj.getObjGen())) {
                JSON::writeDictionaryKey(p, first_object, obj.unparse(), 2);
                obj.writeJSON(1, p, true, 2);
                first_object = false;
            }
        }
        if (all_objects || m->json_objects.count("trailer")) {
            JSON::writeDictionaryKey(p, first_object, "trailer", 2);
            pdf.getTrailer().writeJSON(1, p, true, 2);
            first_object = false;
        }
        JSON::writeDictionaryClose(p, first_object, 1);
    } else {
        std::set<std::string> json_objects;
        if (m->json_objects.count("trailer")) {
            json_objects.insert("trailer");
        }
        for (auto og: getWantedJSONObjects()) {
            json_objects.emplace("obj:" + og.unparse(' ') + " R");
        }
        pdf.writeJSON(
            m->json_version,
            p,
            false,
            first,
            m->decode_level,
            m->json_stream_data,
            m->json_stream_prefix,
            json_objects);
    }
}

void
QPDFJob::doJSONObjectinfo(Pipeline* p, bool& first, QPDF& pdf)
{
    JSON::writeDictionaryKey(p, first, "objectinfo", 1);
    bool first_object = true;
    JSON::writeDictionaryOpen(p, first_object, 1);
    bool all_objects = m->json_objects.empty();
    auto wanted_og = getWantedJSONObjects();
    for (auto& obj: pdf.getAllObjects()) {
        if (all_objects || wanted_og.count(obj.getObjGen())) {
            auto j_details = JSON::makeDictionary();
            auto j_stream = j_details.addDictionaryMember("stream", JSON::makeDictionary());
            bool is_stream = obj.isStream();
            j_stream.addDictionaryMember("is", JSON::makeBool(is_stream));
            j_stream.addDictionaryMember(
                "length",
                (is_stream ? obj.getDict().getKey("/Length").getJSON(m->json_version, true)
                           : JSON::makeNull()));
            j_stream.addDictionaryMember(
                "filter",
                (is_stream ? obj.getDict().getKey("/Filter").getJSON(m->json_version, true)
                           : JSON::makeNull()));
            JSON::writeDictionaryItem(p, first_object, obj.unparse(), j_details, 2);
        }
    }
    JSON::writeDictionaryClose(p, first_object, 1);
}

void
QPDFJob::doJSONPages(Pipeline* p, bool& first, QPDF& pdf)
{
    JSON::writeDictionaryKey(p, first, "pages", 1);
    bool first_page = true;
    JSON::writeArrayOpen(p, first_page, 2);
    QPDFPageLabelDocumentHelper pldh(pdf);
    QPDFOutlineDocumentHelper odh(pdf);
    int pageno = -1;
    for (auto& ph: QPDFPageDocumentHelper(pdf).getAllPages()) {
        ++pageno;
        JSON j_page = JSON::makeDictionary();
        QPDFObjectHandle page = ph.getObjectHandle();
        j_page.addDictionaryMember("object", page.getJSON(m->json_version));
        JSON j_images = j_page.addDictionaryMember("images", JSON::makeArray());
        for (auto const& iter2: ph.getImages()) {
            JSON j_image = j_images.addArrayElement(JSON::makeDictionary());
            j_image.addDictionaryMember("name", JSON::makeString(iter2.first));
            QPDFObjectHandle image = iter2.second;
            QPDFObjectHandle dict = image.getDict();
            j_image.addDictionaryMember("object", image.getJSON(m->json_version));
            j_image.addDictionaryMember("width", dict.getKey("/Width").getJSON(m->json_version));
            j_image.addDictionaryMember("height", dict.getKey("/Height").getJSON(m->json_version));
            j_image.addDictionaryMember(
                "colorspace", dict.getKey("/ColorSpace").getJSON(m->json_version));
            j_image.addDictionaryMember(
                "bitspercomponent", dict.getKey("/BitsPerComponent").getJSON(m->json_version));
            QPDFObjectHandle filters = dict.getKey("/Filter").wrapInArray();
            j_image.addDictionaryMember("filter", filters.getJSON(m->json_version));
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
            j_image.addDictionaryMember("decodeparms", dp_array.getJSON(m->json_version));
            j_image.addDictionaryMember(
                "filterable",
                JSON::makeBool(image.pipeStreamData(nullptr, 0, m->decode_level, true)));
        }
        j_page.addDictionaryMember("images", j_images);
        JSON j_contents = j_page.addDictionaryMember("contents", JSON::makeArray());
        for (auto& iter2: ph.getPageContents()) {
            j_contents.addArrayElement(iter2.getJSON(m->json_version));
        }
        j_page.addDictionaryMember("label", pldh.getLabelForPage(pageno).getJSON(m->json_version));
        JSON j_outlines = j_page.addDictionaryMember("outlines", JSON::makeArray());
        std::vector<QPDFOutlineObjectHelper> outlines = odh.getOutlinesForPage(page.getObjGen());
        for (auto& oiter: outlines) {
            JSON j_outline = j_outlines.addArrayElement(JSON::makeDictionary());
            j_outline.addDictionaryMember(
                "object", oiter.getObjectHandle().getJSON(m->json_version));
            j_outline.addDictionaryMember("title", JSON::makeString(oiter.getTitle()));
            j_outline.addDictionaryMember("dest", oiter.getDest().getJSON(m->json_version, true));
        }
        j_page.addDictionaryMember("pageposfrom1", JSON::makeInt(1 + pageno));
        JSON::writeArrayItem(p, first_page, j_page, 2);
    }
    JSON::writeArrayClose(p, first_page, 1);
}

void
QPDFJob::doJSONPageLabels(Pipeline* p, bool& first, QPDF& pdf)
{
    JSON j_labels = JSON::makeArray();
    QPDFPageLabelDocumentHelper pldh(pdf);
    long long npages = QIntC::to_longlong(QPDFPageDocumentHelper(pdf).getAllPages().size());
    if (pldh.hasPageLabels()) {
        std::vector<QPDFObjectHandle> labels;
        pldh.getLabelsForPageRange(0, npages - 1, 0, labels);
        for (auto iter = labels.begin(); iter != labels.end(); ++iter) {
            if ((iter + 1) == labels.end()) {
                // This can't happen, so ignore it. This could only happen if getLabelsForPageRange
                // somehow returned an odd number of items.
                break;
            }
            JSON j_label = j_labels.addArrayElement(JSON::makeDictionary());
            j_label.addDictionaryMember("index", (*iter).getJSON(m->json_version));
            ++iter;
            j_label.addDictionaryMember("label", (*iter).getJSON(m->json_version));
        }
    }
    JSON::writeDictionaryItem(p, first, "pagelabels", j_labels, 1);
}

void
QPDFJob::addOutlinesToJson(
    std::vector<QPDFOutlineObjectHelper> outlines, JSON& j, std::map<QPDFObjGen, int>& page_numbers)
{
    for (auto& ol: outlines) {
        JSON jo = j.addArrayElement(JSON::makeDictionary());
        jo.addDictionaryMember("object", ol.getObjectHandle().getJSON(m->json_version));
        jo.addDictionaryMember("title", JSON::makeString(ol.getTitle()));
        jo.addDictionaryMember("dest", ol.getDest().getJSON(m->json_version, true));
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
        addOutlinesToJson(ol.getKids(), j_kids, page_numbers);
    }
}

void
QPDFJob::doJSONOutlines(Pipeline* p, bool& first, QPDF& pdf)
{
    std::map<QPDFObjGen, int> page_numbers;
    int n = 0;
    for (auto const& ph: QPDFPageDocumentHelper(pdf).getAllPages()) {
        QPDFObjectHandle oh = ph.getObjectHandle();
        page_numbers[oh.getObjGen()] = ++n;
    }

    JSON j_outlines = JSON::makeArray();
    QPDFOutlineDocumentHelper odh(pdf);
    addOutlinesToJson(odh.getTopLevelOutlines(), j_outlines, page_numbers);
    JSON::writeDictionaryItem(p, first, "outlines", j_outlines, 1);
}

void
QPDFJob::doJSONAcroform(Pipeline* p, bool& first, QPDF& pdf)
{
    JSON j_acroform = JSON::makeDictionary();
    QPDFAcroFormDocumentHelper afdh(pdf);
    j_acroform.addDictionaryMember("hasacroform", JSON::makeBool(afdh.hasAcroForm()));
    j_acroform.addDictionaryMember("needappearances", JSON::makeBool(afdh.getNeedAppearances()));
    JSON j_fields = j_acroform.addDictionaryMember("fields", JSON::makeArray());
    int pagepos1 = 0;
    for (auto const& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        ++pagepos1;
        for (auto& aoh: afdh.getWidgetAnnotationsForPage(page)) {
            QPDFFormFieldObjectHelper ffh = afdh.getFieldForAnnotation(aoh);
            if (!ffh.getObjectHandle().isDictionary()) {
                continue;
            }
            JSON j_field = j_fields.addArrayElement(JSON::makeDictionary());
            j_field.addDictionaryMember("object", ffh.getObjectHandle().getJSON(m->json_version));
            j_field.addDictionaryMember(
                "parent", ffh.getObjectHandle().getKey("/Parent").getJSON(m->json_version));
            j_field.addDictionaryMember("pageposfrom1", JSON::makeInt(pagepos1));
            j_field.addDictionaryMember("fieldtype", JSON::makeString(ffh.getFieldType()));
            j_field.addDictionaryMember("fieldflags", JSON::makeInt(ffh.getFlags()));
            j_field.addDictionaryMember("fullname", JSON::makeString(ffh.getFullyQualifiedName()));
            j_field.addDictionaryMember("partialname", JSON::makeString(ffh.getPartialName()));
            j_field.addDictionaryMember(
                "alternativename", JSON::makeString(ffh.getAlternativeName()));
            j_field.addDictionaryMember("mappingname", JSON::makeString(ffh.getMappingName()));
            j_field.addDictionaryMember("value", ffh.getValue().getJSON(m->json_version));
            j_field.addDictionaryMember(
                "defaultvalue", ffh.getDefaultValue().getJSON(m->json_version));
            j_field.addDictionaryMember("quadding", JSON::makeInt(ffh.getQuadding()));
            j_field.addDictionaryMember("ischeckbox", JSON::makeBool(ffh.isCheckbox()));
            j_field.addDictionaryMember("isradiobutton", JSON::makeBool(ffh.isRadioButton()));
            j_field.addDictionaryMember("ischoice", JSON::makeBool(ffh.isChoice()));
            j_field.addDictionaryMember("istext", JSON::makeBool(ffh.isText()));
            JSON j_choices = j_field.addDictionaryMember("choices", JSON::makeArray());
            for (auto const& choice: ffh.getChoices()) {
                j_choices.addArrayElement(JSON::makeString(choice));
            }
            JSON j_annot = j_field.addDictionaryMember("annotation", JSON::makeDictionary());
            j_annot.addDictionaryMember("object", aoh.getObjectHandle().getJSON(m->json_version));
            j_annot.addDictionaryMember(
                "appearancestate", JSON::makeString(aoh.getAppearanceState()));
            j_annot.addDictionaryMember("annotationflags", JSON::makeInt(aoh.getFlags()));
        }
    }
    JSON::writeDictionaryItem(p, first, "acroform", j_acroform, 1);
}

void
QPDFJob::doJSONEncrypt(Pipeline* p, bool& first, QPDF& pdf)
{
    int R = 0;
    int P = 0;
    int V = 0;
    QPDF::encryption_method_e stream_method = QPDF::e_none;
    QPDF::encryption_method_e string_method = QPDF::e_none;
    QPDF::encryption_method_e file_method = QPDF::e_none;
    bool is_encrypted = pdf.isEncrypted(R, P, V, stream_method, string_method, file_method);
    JSON j_encrypt = JSON::makeDictionary();
    j_encrypt.addDictionaryMember("encrypted", JSON::makeBool(is_encrypted));
    j_encrypt.addDictionaryMember(
        "userpasswordmatched", JSON::makeBool(is_encrypted && pdf.userPasswordMatched()));
    j_encrypt.addDictionaryMember(
        "ownerpasswordmatched", JSON::makeBool(is_encrypted && pdf.ownerPasswordMatched()));
    if (is_encrypted && (V < 5) && pdf.ownerPasswordMatched() && (!pdf.userPasswordMatched())) {
        std::string user_password = pdf.getTrimmedUserPassword();
        j_encrypt.addDictionaryMember("recovereduserpassword", JSON::makeString(user_password));
    } else {
        j_encrypt.addDictionaryMember("recovereduserpassword", JSON::makeNull());
    }
    JSON j_capabilities = j_encrypt.addDictionaryMember("capabilities", JSON::makeDictionary());
    j_capabilities.addDictionaryMember("accessibility", JSON::makeBool(pdf.allowAccessibility()));
    j_capabilities.addDictionaryMember("extract", JSON::makeBool(pdf.allowExtractAll()));
    j_capabilities.addDictionaryMember("printlow", JSON::makeBool(pdf.allowPrintLowRes()));
    j_capabilities.addDictionaryMember("printhigh", JSON::makeBool(pdf.allowPrintHighRes()));
    j_capabilities.addDictionaryMember("modifyassembly", JSON::makeBool(pdf.allowModifyAssembly()));
    j_capabilities.addDictionaryMember("modifyforms", JSON::makeBool(pdf.allowModifyForm()));
    /* cSpell:ignore moddifyannotations */
    std::string MODIFY_ANNOTATIONS =
        (m->json_version == 1 ? "moddifyannotations" : "modifyannotations");
    j_capabilities.addDictionaryMember(
        MODIFY_ANNOTATIONS, JSON::makeBool(pdf.allowModifyAnnotation()));
    j_capabilities.addDictionaryMember("modifyother", JSON::makeBool(pdf.allowModifyOther()));
    j_capabilities.addDictionaryMember("modify", JSON::makeBool(pdf.allowModifyAll()));
    JSON j_parameters = j_encrypt.addDictionaryMember("parameters", JSON::makeDictionary());
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
    auto fix_method = [is_encrypted](QPDF::encryption_method_e& method) {
        if (is_encrypted && method == QPDF::e_none) {
            method = QPDF::e_rc4;
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
    j_parameters.addDictionaryMember("method", JSON::makeString(s_overall_method));
    j_parameters.addDictionaryMember("streammethod", JSON::makeString(s_stream_method));
    j_parameters.addDictionaryMember("stringmethod", JSON::makeString(s_string_method));
    j_parameters.addDictionaryMember("filemethod", JSON::makeString(s_file_method));
    JSON::writeDictionaryItem(p, first, "encrypt", j_encrypt, 1);
}

void
QPDFJob::doJSONAttachments(Pipeline* p, bool& first, QPDF& pdf)
{
    auto to_iso8601 = [](std::string const& d) {
        // Convert PDF date to iso8601 if not empty; if empty, return
        // empty.
        std::string iso8601;
        QUtil::pdf_time_to_iso8601(d, iso8601);
        return iso8601;
    };

    auto null_or_string = [](std::string const& s) {
        if (s.empty()) {
            return JSON::makeNull();
        } else {
            return JSON::makeString(s);
        }
    };

    JSON j_attachments = JSON::makeDictionary();
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    for (auto const& iter: efdh.getEmbeddedFiles()) {
        std::string const& key = iter.first;
        auto fsoh = iter.second;
        auto j_details = j_attachments.addDictionaryMember(key, JSON::makeDictionary());
        j_details.addDictionaryMember(
            "filespec", JSON::makeString(fsoh->getObjectHandle().unparse()));
        j_details.addDictionaryMember("preferredname", JSON::makeString(fsoh->getFilename()));
        j_details.addDictionaryMember(
            "preferredcontents", JSON::makeString(fsoh->getEmbeddedFileStream().unparse()));
        j_details.addDictionaryMember("description", null_or_string(fsoh->getDescription()));
        auto j_names = j_details.addDictionaryMember("names", JSON::makeDictionary());
        for (auto const& i2: fsoh->getFilenames()) {
            j_names.addDictionaryMember(i2.first, JSON::makeString(i2.second));
        }
        auto j_streams = j_details.addDictionaryMember("streams", JSON::makeDictionary());
        for (auto const& [key2, value2]: fsoh->getEmbeddedFileStreams().as_dictionary()) {
            if (value2.null()) {
                continue;
            }
            auto efs = QPDFEFStreamObjectHelper(value2);
            auto j_stream = j_streams.addDictionaryMember(key2, JSON::makeDictionary());
            j_stream.addDictionaryMember(
                "creationdate", null_or_string(to_iso8601(efs.getCreationDate())));
            j_stream.addDictionaryMember(
                "modificationdate", null_or_string(to_iso8601(efs.getCreationDate())));
            j_stream.addDictionaryMember("mimetype", null_or_string(efs.getSubtype()));
            j_stream.addDictionaryMember(
                "checksum", null_or_string(QUtil::hex_encode(efs.getChecksum())));
        }
    }
    JSON::writeDictionaryItem(p, first, "attachments", j_attachments, 1);
}

JSON
QPDFJob::json_schema(int json_version, std::set<std::string>* keys)
{
    // Style: use all lower-case keys with no dashes or underscores. Choose array or dictionary
    // based on indexing. For example, we use a dictionary for objects because we want to index by
    // object ID and an array for pages because we want to index by position. The pages in the pages
    // array contain references back to the original object, which can be resolved in the objects
    // dictionary. When a PDF construct that maps back to an original object is represented
    // separately, use "object" as the key that references the original object.

    // This JSON object doubles as a schema and as documentation for our JSON output. Any schema
    // mismatch is a bug in qpdf. This helps to enforce our policy of consistently providing a known
    // structure where every documented key will always be present, which makes it easier to consume
    // our JSON. This is discussed in more depth in the manual.
    JSON schema = JSON::makeDictionary();
    schema.addDictionaryMember(
        "version",
        JSON::makeString("JSON format serial number; increased for non-compatible changes"));
    JSON j_params = schema.addDictionaryMember("parameters", JSON::parse(R"({
  "decodelevel": "decode level used to determine stream filterability"
})"));

    bool all_keys = ((keys == nullptr) || keys->empty());

    // The list of selectable top-level keys id duplicated in the following places: job.yml,
    // QPDFJob::json_schema, and QPDFJob::doJSON.
    if (json_version == 1) {
        if (all_keys || keys->count("objects")) {
            schema.addDictionaryMember("objects", JSON::parse(R"({
  "<n n R|trailer>": "json representation of object"
})"));
        }
        if (all_keys || keys->count("objectinfo")) {
            JSON objectinfo = schema.addDictionaryMember("objectinfo", JSON::parse(R"({
  "<object-id>": {
    "stream": {
      "filter": "if stream, its filters, otherwise null",
      "is": "whether the object is a stream",
      "length": "if stream, its length, otherwise null"
    }
  }
})"));
        }
    } else {
        if (all_keys || keys->count("qpdf")) {
            schema.addDictionaryMember("qpdf", JSON::parse(R"([{
  "jsonversion": "numeric JSON version",
  "pdfversion": "PDF version as x.y",
  "pushedinheritedpageresources": "whether inherited attributes were pushed to the page level",
  "calledgetallpages": "whether getAllPages was called",
  "maxobjectid": "highest object ID in output, ignored on input"
},
{
  "<obj:n n R|trailer>": "json representation of object"
}])"));
        }
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
    std::string MODIFY_ANNOTATIONS =
        (json_version == 1 ? "moddifyannotations" : "modifyannotations");
    if (all_keys || keys->count("encrypt")) {
        JSON encrypt = schema.addDictionaryMember("encrypt", JSON::parse(R"({
  "capabilities": {
    "accessibility": "allow extraction for accessibility?",
    "extract": "allow extraction?",
    ")" + MODIFY_ANNOTATIONS + R"(": "allow modifying annotations?",
    "modify": "allow all modifications?",
    "modifyassembly": "allow modifying document assembly?",
    "modifyforms": "allow modifying forms?",
    "modifyother": "allow other modifications?",
    "printhigh": "allow high resolution printing?",
    "printlow": "allow low resolution printing?"
  },
  "encrypted": "whether the document is encrypted",
  "ownerpasswordmatched": "whether supplied password matched owner password; always false for non-encrypted files",
  "recovereduserpassword": "If the owner password was used to recover the user password, reveal user password; otherwise null",
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
        JSON attachments = schema.addDictionaryMember("attachments", JSON::parse(R"({
  "<attachment-key>": {
    "filespec": "object containing the file spec",
    "preferredcontents": "most preferred embedded file stream",
    "preferredname": "most preferred file name",
    "description": "description of attachment",
    "names": {
      "<name-key>": "file name for key"
    },
    "streams": {
      "<stream-key>": {
        "creationdate": "ISO-8601 creation date or null",
        "modificationdate": "ISO-8601 modification date or null",
        "mimetype": "mime type or null",
        "checksum": "MD5 checksum or null"
      }
    }
  }
})"));
    }
    return schema;
}

std::string
QPDFJob::json_out_schema(int version)
{
    return json_schema(version).unparse();
}

std::string
QPDFJob::json_out_schema_v1()
{
    return json_schema(1).unparse();
}

void
QPDFJob::doJSON(QPDF& pdf, Pipeline* p)
{
    // qpdf guarantees that no new top-level keys whose names start with "x-" will be added. These
    // are reserved for users.

    std::string captured_json;
    std::shared_ptr<Pl_String> pl_str;
    if (m->test_json_schema) {
        pl_str = std::make_shared<Pl_String>("capture json", p, captured_json);
        p = pl_str.get();
    }

    bool first = true;
    JSON::writeDictionaryOpen(p, first, 0);

    if (m->json_output) {
        // Exclude version and parameters to keep the output file minimal. The JSON version is
        // inside the "qpdf" key for version 2.
    } else {
        // This version is updated every time a non-backward-compatible change is made to the JSON
        // format. Clients of the JSON are to ignore unrecognized keys, so we only update the
        // version of a key disappears or if its value changes meaning.
        JSON::writeDictionaryItem(p, first, "version", JSON::makeInt(m->json_version), 1);
        JSON j_params = JSON::makeDictionary();
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
        j_params.addDictionaryMember("decodelevel", JSON::makeString(decode_level_str));
        JSON::writeDictionaryItem(p, first, "parameters", j_params, 1);
    }
    bool all_keys = m->json_keys.empty();
    // The list of selectable top-level keys id duplicated in the following places: job.yml,
    // QPDFJob::json_schema, and QPDFJob::doJSON.

    // We do pages and pagelabels first since they have the side effect of repairing the pages tree,
    // which could potentially impact object references in remaining items.
    if (all_keys || m->json_keys.count("pages")) {
        doJSONPages(p, first, pdf);
    }
    if (all_keys || m->json_keys.count("pagelabels")) {
        doJSONPageLabels(p, first, pdf);
    }

    // The non-special keys are output in alphabetical order, but the order doesn't actually matter.
    if (all_keys || m->json_keys.count("acroform")) {
        doJSONAcroform(p, first, pdf);
    }
    if (all_keys || m->json_keys.count("attachments")) {
        doJSONAttachments(p, first, pdf);
    }
    if (all_keys || m->json_keys.count("encrypt")) {
        doJSONEncrypt(p, first, pdf);
    }
    if (all_keys || m->json_keys.count("outlines")) {
        doJSONOutlines(p, first, pdf);
    }

    // We do objects last so their information is consistent with repairing the page tree. To see
    // the original file with any page tree problems and the page tree not flattened, select
    // qpdf/objects/objectinfo without other keys.
    if (all_keys || m->json_keys.count("objects") || m->json_keys.count("qpdf")) {
        doJSONObjects(p, first, pdf);
    }
    if (m->json_version == 1) {
        // "objectinfo" is not needed for version >1 since you can tell streams from other objects
        // in "objects".
        if (all_keys || m->json_keys.count("objectinfo")) {
            doJSONObjectinfo(p, first, pdf);
        }
    }

    JSON::writeDictionaryClose(p, first, 0);
    *p << "\n";

    if (m->test_json_schema) {
        // Check against schema
        JSON schema = json_schema(m->json_version, &m->json_keys);
        std::list<std::string> errors;
        JSON captured = JSON::parse(captured_json);
        if (!captured.checkSchema(schema, errors)) {
            m->log->error("QPDFJob didn't create JSON that complies with its own rules.\n");
            for (auto const& error: errors) {
                *m->log->getError() << error << "\n";
            }
        }
    }
}

void
QPDFJob::doInspection(QPDF& pdf)
{
    auto& cout = *m->log->getInfo();
    if (m->check) {
        doCheck(pdf);
    }
    if (m->show_npages) {
        QTC::TC("qpdf", "QPDFJob npages");
        cout << pdf.getRoot().getKey("/Pages").getKey("/Count").getIntValue() << "\n";
    }
    if (m->show_encryption) {
        showEncryption(pdf);
    }
    if (m->check_linearization) {
        if (!pdf.isLinearized()) {
            cout << m->infilename.get() << " is not linearized\n";
        } else if (pdf.checkLinearization()) {
            cout << m->infilename.get() << ": no linearization errors\n";
        } else {
            m->warnings = true;
        }
    }
    if (m->show_linearization) {
        if (pdf.isLinearized()) {
            pdf.showLinearizationData();
        } else {
            cout << m->infilename.get() << " is not linearized\n";
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
        m->warnings = true;
    }
}

void
QPDFJob::doProcessOnce(
    std::unique_ptr<QPDF>& pdf,
    std::function<void(QPDF*, char const*)> fn,
    char const* password,
    bool empty,
    bool used_for_input,
    bool main_input)
{
    pdf = std::make_unique<QPDF>();
    setQPDFOptions(*pdf);
    if (empty) {
        pdf->emptyPDF();
    } else if (main_input && m->json_input) {
        pdf->createFromJSON(m->infilename.get());
    } else {
        fn(pdf.get(), password);
    }
    if (used_for_input) {
        m->max_input_version.updateIfGreater(pdf->getVersionAsPDFVersion());
    }
}

void
QPDFJob::doProcess(
    std::unique_ptr<QPDF>& pdf,
    std::function<void(QPDF*, char const*)> fn,
    char const* password,
    bool empty,
    bool used_for_input,
    bool main_input)
{
    // If a password has been specified but doesn't work, try other passwords that are equivalent in
    // different character encodings. This makes it possible to open PDF files that were encrypted
    // using incorrect string encodings. For example, if someone used a password encoded in PDF Doc
    // encoding or Windows code page 1252 for an AES-encrypted file or a UTF-8-encoded password on
    // an RC4-encrypted file, or if the password was properly encoded but the password given here
    // was incorrectly encoded, there's a good chance we'd succeed here.

    std::string ptemp;
    if (password && (!m->password_is_hex_key)) {
        if (m->password_mode == QPDFJob::pm_hex_bytes) {
            // Special case: handle --password-mode=hex-bytes for input password as well as output
            // password
            QTC::TC("qpdf", "QPDFJob input password hex-bytes");
            ptemp = QUtil::hex_decode(password);
            password = ptemp.c_str();
        }
    }
    if ((password == nullptr) || empty || m->password_is_hex_key || m->suppress_password_recovery) {
        // There is no password, or we're not doing recovery, so just do the normal processing with
        // the supplied password.
        doProcessOnce(pdf, fn, password, empty, used_for_input, main_input);
        return;
    }

    // Get a list of otherwise encoded strings. Keep in scope for this method.
    std::vector<std::string> passwords_str = QUtil::possible_repaired_encodings(password);
    // Represent to char const*, as required by the QPDF class.
    std::vector<char const*> passwords;
    for (auto const& iter: passwords_str) {
        passwords.push_back(iter.c_str());
    }
    // We always try the supplied password first because it is the first string returned by
    // possible_repaired_encodings. If there is more than one option, go ahead and put the supplied
    // password at the end so that it's that decoding attempt whose exception is thrown.
    if (passwords.size() > 1) {
        passwords.push_back(password);
    }

    // Try each password. If one works, return the resulting object. If they all fail, throw the
    // exception thrown by the final attempt, which, like the first attempt, will be with the
    // supplied password.
    bool warned = false;
    for (auto iter = passwords.begin(); iter != passwords.end(); ++iter) {
        try {
            doProcessOnce(pdf, fn, *iter, empty, used_for_input, main_input);
            return;
        } catch (QPDFExc&) {
            auto next = iter;
            ++next;
            if (next == passwords.end()) {
                throw;
            }
        }
        if (!warned) {
            warned = true;
            doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                v << prefix
                  << ": supplied password didn't work; trying other passwords based on "
                     "interpreting password with different string encodings\n";
            });
        }
    }
    // Should not be reachable
    throw std::logic_error("do_process returned");
}

void
QPDFJob::processFile(
    std::unique_ptr<QPDF>& pdf,
    char const* filename,
    char const* password,
    bool used_for_input,
    bool main_input)
{
    auto f1 = std::mem_fn<void(char const*, char const*)>(&QPDF::processFile);
    auto fn = std::bind(f1, std::placeholders::_1, filename, std::placeholders::_2);
    doProcess(pdf, fn, password, strcmp(filename, "") == 0, used_for_input, main_input);
}

void
QPDFJob::processInputSource(
    std::unique_ptr<QPDF>& pdf,
    std::shared_ptr<InputSource> is,
    char const* password,
    bool used_for_input)
{
    auto f1 = std::mem_fn(&QPDF::processInputSource);
    auto fn = std::bind(f1, std::placeholders::_1, is, std::placeholders::_2);
    doProcess(pdf, fn, password, false, used_for_input, false);
}

void
QPDFJob::validateUnderOverlay(QPDF& pdf, UnderOverlay* uo)
{
    QPDFPageDocumentHelper main_pdh(pdf);
    int main_npages = QIntC::to_int(main_pdh.getAllPages().size());
    processFile(uo->pdf, uo->filename.c_str(), uo->password.get(), true, false);
    QPDFPageDocumentHelper uo_pdh(*(uo->pdf));
    int uo_npages = QIntC::to_int(uo_pdh.getAllPages().size());
    try {
        uo->to_pagenos = QUtil::parse_numrange(uo->to_nr.c_str(), main_npages);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(
            "parsing numeric range for " + uo->which + " \"to\" pages: " + e.what());
    }
    try {
        if (uo->from_nr.empty()) {
            QTC::TC("qpdf", "QPDFJob from_nr from repeat_nr");
            uo->from_nr = uo->repeat_nr;
        }
        uo->from_pagenos = QUtil::parse_numrange(uo->from_nr.c_str(), uo_npages);
        if (!uo->repeat_nr.empty()) {
            uo->repeat_pagenos = QUtil::parse_numrange(uo->repeat_nr.c_str(), uo_npages);
        }
    } catch (std::runtime_error& e) {
        throw std::runtime_error(
            "parsing numeric range for " + uo->which + " file " + uo->filename + ": " + e.what());
    }
}

static QPDFAcroFormDocumentHelper*
get_afdh_for_qpdf(
    std::map<unsigned long long, std::shared_ptr<QPDFAcroFormDocumentHelper>>& afdh_map, QPDF* q)
{
    auto uid = q->getUniqueId();
    if (!afdh_map.count(uid)) {
        afdh_map[uid] = std::make_shared<QPDFAcroFormDocumentHelper>(*q);
    }
    return afdh_map[uid].get();
}

std::string
QPDFJob::doUnderOverlayForPage(
    QPDF& pdf,
    UnderOverlay& uo,
    std::map<int, std::map<size_t, std::vector<int>>>& pagenos,
    size_t page_idx,
    size_t uo_idx,
    std::map<int, std::map<size_t, QPDFObjectHandle>>& fo,
    std::vector<QPDFPageObjectHelper>& pages,
    QPDFPageObjectHelper& dest_page)
{
    int pageno = 1 + QIntC::to_int(page_idx);
    if (!(pagenos.count(pageno) && pagenos[pageno].count(uo_idx))) {
        return "";
    }

    std::map<unsigned long long, std::shared_ptr<QPDFAcroFormDocumentHelper>> afdh;
    auto make_afdh = [&](QPDFPageObjectHelper& ph) {
        QPDF& q = ph.getObjectHandle().getQPDF();
        return get_afdh_for_qpdf(afdh, &q);
    };
    auto dest_afdh = make_afdh(dest_page);

    std::string content;
    int min_suffix = 1;
    QPDFObjectHandle resources = dest_page.getAttribute("/Resources", true);
    for (int from_pageno: pagenos[pageno][uo_idx]) {
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << "    " << uo.filename << " " << uo.which << " " << from_pageno << "\n";
        });
        auto from_page = pages.at(QIntC::to_size(from_pageno - 1));
        if (fo[from_pageno].count(uo_idx) == 0) {
            fo[from_pageno][uo_idx] = pdf.copyForeignObject(from_page.getFormXObjectForPage());
        }

        // If the same page is overlaid or underlaid multiple times, we'll generate multiple names
        // for it, but that's harmless and also a pretty goofy case that's not worth coding around.
        std::string name = resources.getUniqueResourceName("/Fx", min_suffix);
        QPDFMatrix cm;
        std::string new_content = dest_page.placeFormXObject(
            fo[from_pageno][uo_idx], name, dest_page.getTrimBox().getArrayAsRectangle(), cm);
        dest_page.copyAnnotations(from_page, cm, dest_afdh, make_afdh(from_page));
        if (!new_content.empty()) {
            resources.mergeResources("<< /XObject << >> >>"_qpdf);
            auto xobject = resources.getKey("/XObject");
            if (xobject.isDictionary()) {
                xobject.replaceKey(name, fo[from_pageno][uo_idx]);
            }
            ++min_suffix;
            content += new_content;
        }
    }
    return content;
}

void
QPDFJob::getUOPagenos(
    std::vector<QPDFJob::UnderOverlay>& uos,
    std::map<int, std::map<size_t, std::vector<int>>>& pagenos)
{
    size_t uo_idx = 0;
    for (auto const& uo: uos) {
        size_t page_idx = 0;
        size_t from_size = uo.from_pagenos.size();
        size_t repeat_size = uo.repeat_pagenos.size();
        for (int to_pageno: uo.to_pagenos) {
            if (page_idx < from_size) {
                pagenos[to_pageno][uo_idx].push_back(uo.from_pagenos.at(page_idx));
            } else if (repeat_size) {
                pagenos[to_pageno][uo_idx].push_back(
                    uo.repeat_pagenos.at((page_idx - from_size) % repeat_size));
            }
            ++page_idx;
        }
        ++uo_idx;
    }
}

void
QPDFJob::handleUnderOverlay(QPDF& pdf)
{
    if (m->underlay.empty() && m->overlay.empty()) {
        return;
    }
    for (auto& uo: m->underlay) {
        validateUnderOverlay(pdf, &uo);
    }
    for (auto& uo: m->overlay) {
        validateUnderOverlay(pdf, &uo);
    }

    // First map key is 1-based page number. Second is index into the overlay/underlay vector. Watch
    // out to not reverse the keys or be off by one.
    std::map<int, std::map<size_t, std::vector<int>>> underlay_pagenos;
    std::map<int, std::map<size_t, std::vector<int>>> overlay_pagenos;
    getUOPagenos(m->underlay, underlay_pagenos);
    getUOPagenos(m->overlay, overlay_pagenos);
    doIfVerbose([&](Pipeline& v, std::string const& prefix) {
        v << prefix << ": processing underlay/overlay\n";
    });

    auto get_pages = [](std::vector<UnderOverlay>& v,
                        std::vector<std::vector<QPDFPageObjectHelper>>& v_out) {
        for (auto const& uo: v) {
            if (uo.pdf) {
                v_out.push_back(QPDFPageDocumentHelper(*(uo.pdf)).getAllPages());
            }
        }
    };
    std::vector<std::vector<QPDFPageObjectHelper>> upages;
    get_pages(m->underlay, upages);
    std::vector<std::vector<QPDFPageObjectHelper>> opages;
    get_pages(m->overlay, opages);

    std::map<int, std::map<size_t, QPDFObjectHandle>> underlay_fo;
    std::map<int, std::map<size_t, QPDFObjectHandle>> overlay_fo;
    QPDFPageDocumentHelper main_pdh(pdf);
    auto main_pages = main_pdh.getAllPages();
    size_t main_npages = main_pages.size();
    for (size_t page_idx = 0; page_idx < main_npages; ++page_idx) {
        auto pageno = QIntC::to_int(page_idx) + 1;
        doIfVerbose(
            [&](Pipeline& v, std::string const& prefix) { v << "  page " << pageno << "\n"; });
        if (underlay_pagenos[pageno].empty() && overlay_pagenos[pageno].empty()) {
            continue;
        }
        // This code converts the original page, any underlays, and any overlays to form XObjects.
        // Then it concatenates display of all underlays, the original page, and all overlays. Prior
        // to 11.3.0, the original page contents were wrapped in q/Q, but this didn't work if the
        // original page had unbalanced q/Q operators. See GitHub issue #904.
        auto& dest_page = main_pages.at(page_idx);
        auto dest_page_oh = dest_page.getObjectHandle();
        auto this_page_fo = dest_page.getFormXObjectForPage();
        // The resulting form xobject lazily reads the content from the original page, which we are
        // going to replace. Therefore, we have to explicitly copy it.
        auto content_data = this_page_fo.getRawStreamData();
        this_page_fo.replaceStreamData(content_data, QPDFObjectHandle(), QPDFObjectHandle());
        auto resources =
            dest_page_oh.replaceKeyAndGetNew("/Resources", "<< /XObject << >> >>"_qpdf);
        resources.getKey("/XObject").replaceKeyAndGetNew("/Fx0", this_page_fo);
        size_t uo_idx{0};
        std::string content;
        for (auto& underlay: m->underlay) {
            content += doUnderOverlayForPage(
                pdf,
                underlay,
                underlay_pagenos,
                page_idx,
                uo_idx,
                underlay_fo,
                upages[uo_idx],
                dest_page);
            ++uo_idx;
        }
        content += dest_page.placeFormXObject(
            this_page_fo,
            "/Fx0",
            dest_page.getMediaBox().getArrayAsRectangle(),
            true,
            false,
            false);
        uo_idx = 0;
        for (auto& overlay: m->overlay) {
            content += doUnderOverlayForPage(
                pdf,
                overlay,
                overlay_pagenos,
                page_idx,
                uo_idx,
                overlay_fo,
                opages[uo_idx],
                dest_page);
            ++uo_idx;
        }
        dest_page_oh.replaceKey("/Contents", pdf.newStream(content));
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

        auto fs = QPDFFileSpecObjectHelper::createFileSpec(pdf, to_add.filename, to_add.path);
        if (!to_add.description.empty()) {
            fs.setDescription(to_add.description);
        }
        auto efs = QPDFEFStreamObjectHelper(fs.getEmbeddedFileStream());
        efs.setCreationDate(to_add.creationdate).setModDate(to_add.moddate);
        if (!to_add.mimetype.empty()) {
            efs.setSubtype(to_add.mimetype);
        }

        efdh.replaceEmbeddedFile(to_add.key, fs);
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": attached " << to_add.path << " as " << to_add.filename << " with key "
              << to_add.key << "\n";
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
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": copying attachments from " << to_copy.path << "\n";
        });
        std::unique_ptr<QPDF> other;
        processFile(other, to_copy.path.c_str(), to_copy.password.c_str(), false, false);
        QPDFEmbeddedFileDocumentHelper other_efdh(*other);
        auto other_attachments = other_efdh.getEmbeddedFiles();
        for (auto const& iter: other_attachments) {
            std::string new_key = to_copy.prefix + iter.first;
            if (efdh.getEmbeddedFile(new_key)) {
                duplicates.push_back("file: " + to_copy.path + ", key: " + new_key);
            } else {
                auto new_fs_oh = pdf.copyForeignObject(iter.second->getObjectHandle());
                efdh.replaceEmbeddedFile(new_key, QPDFFileSpecObjectHelper(new_fs_oh));
                doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                    v << "  " << iter.first << " -> " << new_key << "\n";
                });
            }
        }

        if (other->anyWarnings()) {
            m->warnings = true;
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
            " already has attachments with keys that conflict with attachments from other files: " +
            message +
            ". Use --prefix with --copy-attachments-from or manually copy individual attachments.";
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
    if (m->remove_restrictions) {
        make_afdh();
        afdh->disableDigitalSignatures();
    }
    if (m->externalize_inline_images || (m->optimize_images && (!m->keep_inline_images))) {
        for (auto& ph: dh.getAllPages()) {
            ph.externalizeInlineImages(m->ii_min_bytes);
        }
    }
    if (m->optimize_images) {
        int pageno = 0;
        for (auto& ph: dh.getAllPages()) {
            ++pageno;
            ph.forEachImage(
                true,
                [this, pageno, &pdf](
                    QPDFObjectHandle& obj, QPDFObjectHandle& xobj_dict, std::string const& key) {
                    auto io = std::make_unique<ImageOptimizer>(
                        *this,
                        m->oi_min_width,
                        m->oi_min_height,
                        m->oi_min_area,
                        m->jpeg_quality,
                        obj);
                    if (io->evaluate("image " + key + " on page " + std::to_string(pageno))) {
                        QPDFObjectHandle new_image = pdf.newStream();
                        new_image.replaceDict(obj.getDict().shallowCopy());
                        new_image.replaceStreamData(
                            std::move(io),
                            QPDFObjectHandle::newName("/DCTDecode"),
                            QPDFObjectHandle::newNull());
                        xobj_dict.replaceKey(key, new_image);
                    }
                });
        }
    }
    if (m->generate_appearances) {
        make_afdh();
        afdh->generateAppearancesIfNeeded();
    }
    if (m->flatten_annotations) {
        dh.flattenAnnotations(m->flatten_annotations_required, m->flatten_annotations_forbidden);
    }
    if (m->coalesce_contents) {
        for (auto& page: dh.getAllPages()) {
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
    if (!m->page_label_specs.empty()) {
        auto nums = QPDFObjectHandle::newArray();
        auto n_pages = QIntC::to_int(dh.getAllPages().size());
        int last_page_seen{0};
        for (auto& spec: m->page_label_specs) {
            if (spec.first_page < 0) {
                spec.first_page = n_pages + 1 + spec.first_page;
            }
            if (last_page_seen == 0) {
                if (spec.first_page != 1) {
                    throw std::runtime_error(
                        "the first page label specification must start with page 1");
                }
            } else if (spec.first_page <= last_page_seen) {
                throw std::runtime_error(
                    "page label specifications must be in order by first page");
            }
            if (spec.first_page > n_pages) {
                throw std::runtime_error(
                    "page label spec: page " + std::to_string(spec.first_page) +
                    " is more than the total number of pages (" + std::to_string(n_pages) + ")");
            }
            last_page_seen = spec.first_page;
            nums.appendItem(QPDFObjectHandle::newInteger(spec.first_page - 1));
            nums.appendItem(
                QPDFPageLabelDocumentHelper::pageLabelDict(
                    spec.label_type, spec.start_num, spec.prefix));
        }
        auto page_labels = QPDFObjectHandle::newDictionary();
        page_labels.replaceKey("/Nums", nums);
        pdf.getRoot().replaceKey("/PageLabels", page_labels);
    }
    if (!m->attachments_to_remove.empty()) {
        QPDFEmbeddedFileDocumentHelper efdh(pdf);
        for (auto const& key: m->attachments_to_remove) {
            if (efdh.removeEmbeddedFile(key)) {
                doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                    v << prefix << ": removed attachment " << key << "\n";
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

    // Unreferenced resources are common in files where resources dictionaries are shared across
    // pages. As a heuristic, we look in the file for shared resources dictionaries or shared
    // XObject subkeys of resources dictionaries either on pages or on form XObjects in pages. If we
    // find any, then there is a higher likelihood that the expensive process of finding
    // unreferenced resources is worth it.

    // Return true as soon as we find any shared resources.

    QPDFObjGen::set resources_seen; // shared resources detection
    QPDFObjGen::set nodes_seen;     // loop detection

    doIfVerbose([&](Pipeline& v, std::string const& prefix) {
        v << prefix << ": " << pdf.getFilename() << ": checking for shared resources\n";
    });

    std::list<QPDFObjectHandle> queue;
    queue.push_back(pdf.getRoot().getKey("/Pages"));
    while (!queue.empty()) {
        QPDFObjectHandle node = *queue.begin();
        queue.pop_front();
        QPDFObjGen og = node.getObjGen();
        if (!nodes_seen.add(og)) {
            continue;
        }
        QPDFObjectHandle dict = node.isStream() ? node.getDict() : node;
        QPDFObjectHandle kids = dict.getKey("/Kids");
        if (kids.isArray()) {
            // This is a non-leaf node.
            if (dict.hasKey("/Resources")) {
                QTC::TC("qpdf", "QPDFJob found resources in non-leaf");
                doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                    v << "  found resources in non-leaf page node " << og.unparse(' ') << "\n";
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
                if (!resources_seen.add(resources)) {
                    QTC::TC("qpdf", "QPDFJob found shared resources in leaf");
                    doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                        v << "  found shared resources in leaf node " << og.unparse(' ') << ": "
                          << resources.getObjGen().unparse(' ') << "\n";
                    });
                    return true;
                }
            }
            QPDFObjectHandle xobject =
                (resources.isDictionary() ? resources.getKey("/XObject")
                                          : QPDFObjectHandle::newNull());
            if (xobject.isIndirect()) {
                if (!resources_seen.add(xobject)) {
                    QTC::TC("qpdf", "QPDFJob found shared xobject in leaf");
                    doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                        v << "  found shared xobject in leaf node " << og.unparse(' ') << ": "
                          << xobject.getObjGen().unparse(' ') << "\n";
                    });
                    return true;
                }
            }

            for (auto const& xobj: xobject.as_dictionary()) {
                if (xobj.second.isFormXObject()) {
                    queue.emplace_back(xobj.second);
                }
            }
        }
    }

    doIfVerbose([&](Pipeline& v, std::string const& prefix) {
        v << prefix << ": no shared resources found\n";
    });
    return false;
}

static QPDFObjectHandle
added_page(QPDF& pdf, QPDFObjectHandle page)
{
    QPDFObjectHandle result = page;
    if (&page.getQPDF() != &pdf) {
        // Calling copyForeignObject on an object we already copied will give us the already
        // existing copy.
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
QPDFJob::handlePageSpecs(QPDF& pdf, std::vector<std::unique_ptr<QPDF>>& page_heap)
{
    // Parse all page specifications and translate them into lists of actual pages.

    // Handle "." as a shortcut for the input file
    for (auto& page_spec: m->page_specs) {
        if (page_spec.filename == ".") {
            page_spec.filename = m->infilename.get();
        }
        if (page_spec.range.empty()) {
            page_spec.range = "1-z";
        }
    }

    if (!m->keep_files_open_set) {
        // Count the number of distinct files to determine whether we should keep files open or not.
        // Rather than trying to code some portable heuristic based on OS limits, just hard-code
        // this at a given number and allow users to override.
        std::set<std::string> filenames;
        for (auto& page_spec: m->page_specs) {
            filenames.insert(page_spec.filename);
        }
        m->keep_files_open = (filenames.size() <= m->keep_files_open_threshold);
        QTC::TC("qpdf", "QPDFJob automatically set keep files open", m->keep_files_open ? 0 : 1);
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": selecting --keep-open-files=" << (m->keep_files_open ? "y" : "n")
              << "\n";
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
            // Open the PDF file and store the QPDF object. Throw a std::shared_ptr to the qpdf into
            // a heap so that it survives through copying to the output but gets cleaned up
            // automatically at the end. Do not canonicalize the file name. Using two different
            // paths to refer to the same file is a documented workaround for duplicating a page. If
            // you are using this an example of how to do this with the API, you can just create two
            // different QPDF objects to the same underlying file with the same path to achieve the
            // same effect.
            char const* password = page_spec.password.get();
            if ((!m->encryption_file.empty()) && (password == nullptr) &&
                (page_spec.filename == m->encryption_file)) {
                QTC::TC("qpdf", "QPDFJob pages encryption password");
                password = m->encryption_file_password.get();
            }
            doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                v << prefix << ": processing " << page_spec.filename << "\n";
            });
            std::shared_ptr<InputSource> is;
            ClosedFileInputSource* cis = nullptr;
            if (!m->keep_files_open) {
                QTC::TC("qpdf", "QPDFJob keep files open n");
                cis = new ClosedFileInputSource(page_spec.filename.c_str());
                is = std::shared_ptr<InputSource>(cis);
                cis->stayOpen(true);
            } else {
                QTC::TC("qpdf", "QPDFJob keep files open y");
                FileInputSource* fis = new FileInputSource(page_spec.filename.c_str());
                is = std::shared_ptr<InputSource>(fis);
            }
            std::unique_ptr<QPDF> qpdf_sp;
            processInputSource(qpdf_sp, is, password, true);
            page_spec_qpdfs[page_spec.filename] = qpdf_sp.get();
            page_heap.push_back(std::move(qpdf_sp));
            if (cis) {
                cis->stayOpen(false);
                page_spec_cfis[page_spec.filename] = cis;
            }
        }

        // Read original pages from the PDF, and parse the page range associated with this
        // occurrence of the file.
        parsed_specs.emplace_back(
            page_spec.filename, page_spec_qpdfs[page_spec.filename], page_spec.range);
    }

    std::map<unsigned long long, bool> remove_unreferenced;
    if (m->remove_unreferenced_page_resources != QPDFJob::re_no) {
        for (auto const& iter: page_spec_qpdfs) {
            std::string const& filename = iter.first;
            ClosedFileInputSource* cis = nullptr;
            if (page_spec_cfis.count(filename)) {
                cis = page_spec_cfis[filename];
                cis->stayOpen(true);
            }
            QPDF& other(*(iter.second));
            auto other_uuid = other.getUniqueId();
            if (remove_unreferenced.count(other_uuid) == 0) {
                remove_unreferenced[other_uuid] = shouldRemoveUnreferencedResources(other);
            }
            if (cis) {
                cis->stayOpen(false);
            }
        }
    }

    // Clear all pages out of the primary QPDF's pages tree but leave the objects in place in the
    // file so they can be re-added without changing their object numbers. This enables other things
    // in the original file, such as outlines, to continue to work.
    doIfVerbose([&](Pipeline& v, std::string const& prefix) {
        v << prefix << ": removing unreferenced pages from primary input\n";
    });
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> orig_pages = dh.getAllPages();
    for (auto const& page: orig_pages) {
        dh.removePage(page);
    }

    auto n_collate = m->collate.size();
    auto n_specs = parsed_specs.size();
    if (!(n_collate == 0 || n_collate == 1 || n_collate == n_specs)) {
        usage(
            "--pages: if --collate has more than one value, it must have one value per page "
            "specification");
    }
    if (n_collate > 0 && n_specs > 1) {
        // Collate the pages by selecting one page from each spec in order. When a spec runs out of
        // pages, stop selecting from it.
        std::vector<QPDFPageData> new_parsed_specs;
        // Make sure we have a collate value for each spec. We have already checked that a non-empty
        // collate has either one value or one value per spec.
        for (auto i = n_collate; i < n_specs; ++i) {
            m->collate.push_back(m->collate.at(0));
        }
        std::vector<size_t> cur_page(n_specs, 0);
        bool got_pages = true;
        while (got_pages) {
            got_pages = false;
            for (size_t i = 0; i < n_specs; ++i) {
                QPDFPageData& page_data = parsed_specs.at(i);
                for (size_t j = 0; j < m->collate.at(i); ++j) {
                    if (cur_page.at(i) + j < page_data.selected_pages.size()) {
                        got_pages = true;
                        new_parsed_specs.emplace_back(
                            page_data, page_data.selected_pages.at(cur_page.at(i) + j));
                    }
                }
                cur_page.at(i) += m->collate.at(i);
            }
        }
        parsed_specs = new_parsed_specs;
    }

    // Add all the pages from all the files in the order specified. Keep track of any pages from the
    // original file that we are selecting.
    std::set<int> selected_from_orig;
    std::vector<QPDFObjectHandle> new_labels;
    bool any_page_labels = false;
    int out_pageno = 0;
    std::map<unsigned long long, std::shared_ptr<QPDFAcroFormDocumentHelper>> afdh_map;
    auto this_afdh = get_afdh_for_qpdf(afdh_map, &pdf);
    std::set<QPDFObjGen> referenced_fields;
    for (auto& page_data: parsed_specs) {
        ClosedFileInputSource* cis = nullptr;
        if (page_spec_cfis.count(page_data.filename)) {
            cis = page_spec_cfis[page_data.filename];
            cis->stayOpen(true);
        }
        QPDFPageLabelDocumentHelper pldh(*page_data.qpdf);
        auto other_afdh = get_afdh_for_qpdf(afdh_map, page_data.qpdf);
        if (pldh.hasPageLabels()) {
            any_page_labels = true;
        }
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": adding pages from " << page_data.filename << "\n";
        });
        for (auto pageno_iter: page_data.selected_pages) {
            // Pages are specified from 1 but numbered from 0 in the vector
            int pageno = pageno_iter - 1;
            pldh.getLabelsForPageRange(pageno, pageno, out_pageno++, new_labels);
            QPDFPageObjectHelper to_copy = page_data.orig_pages.at(QIntC::to_size(pageno));
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
                // This is a page from the original file. Keep track of the fact that we are using
                // it.
                first_copy_from_orig = (selected_from_orig.count(pageno) == 0);
                selected_from_orig.insert(pageno);
            }
            auto new_page = added_page(pdf, to_copy);
            // Try to avoid gratuitously renaming fields. In the case of where we're just extracting
            // a bunch of pages from the original file and not copying any page more than once,
            // there's no reason to do anything with the fields. Since we don't remove fields from
            // the original file until all copy operations are completed, any foreign pages that
            // conflict with original pages will be adjusted. If we copy any page from the original
            // file more than once, that page would be in conflict with the previous copy of itself.
            if ((!this_file && other_afdh->hasAcroForm()) || !first_copy_from_orig) {
                if (!this_file) {
                    QTC::TC("qpdf", "QPDFJob copy fields not this file");
                } else if (!first_copy_from_orig) {
                    QTC::TC("qpdf", "QPDFJob copy fields non-first from orig");
                }
                try {
                    this_afdh->fixCopiedAnnotations(
                        new_page, to_copy.getObjectHandle(), *other_afdh, &referenced_fields);
                } catch (std::exception& e) {
                    pdf.warn(
                        qpdf_e_damaged_pdf,
                        "",
                        0,
                        ("Exception caught while fixing copied annotations. This may be a qpdf "
                         "bug. " +
                         std::string("Exception: ") + e.what()));
                }
            }
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

    // Delete page objects for unused page in primary. This prevents those objects from being
    // preserved by being referred to from other places, such as the outlines dictionary. Also make
    // sure we keep form fields from pages we preserved.
    for (size_t pageno = 0; pageno < orig_pages.size(); ++pageno) {
        auto page = orig_pages.at(pageno);
        if (selected_from_orig.count(QIntC::to_int(pageno))) {
            for (auto field: this_afdh->getFormFieldsForPage(page)) {
                QTC::TC("qpdf", "QPDFJob pages keeping field from original");
                referenced_fields.insert(field.getObjectHandle().getObjGen());
            }
        } else {
            pdf.replaceObject(page.getObjectHandle().getObjGen(), QPDFObjectHandle::newNull());
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
        for (int pageno_iter: QUtil::parse_numrange(range.c_str(), npages)) {
            int pageno = pageno_iter - 1;
            if ((pageno >= 0) && (pageno < npages)) {
                pages.at(QIntC::to_size(pageno)).rotatePage(rspec.angle, rspec.relative);
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
            QUtil::analyze_encoding(password, has_8bit_chars, is_valid_utf8, is_utf16);
            if (!has_8bit_chars) {
                return;
            }
            if (m->password_mode == QPDFJob::pm_unicode) {
                if (!is_valid_utf8) {
                    QTC::TC("qpdf", "QPDFJob password not unicode");
                    throw std::runtime_error("supplied password is not valid UTF-8");
                }
                if (R < 5) {
                    std::string encoded;
                    if (!QUtil::utf8_to_pdf_doc(password, encoded)) {
                        QTC::TC("qpdf", "QPDFJob password not encodable");
                        throw std::runtime_error(
                            "supplied password cannot be encoded for 40-bit "
                            "or 128-bit encryption formats");
                    }
                    password = encoded;
                }
            } else {
                if ((R < 5) && is_valid_utf8) {
                    std::string encoded;
                    if (QUtil::utf8_to_pdf_doc(password, encoded)) {
                        QTC::TC("qpdf", "QPDFJob auto-encode password");
                        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
                            v << prefix
                              << ": automatically converting Unicode password to single-byte "
                                 "encoding as required for 40-bit or 128-bit encryption\n";
                        });
                        password = encoded;
                    } else {
                        QTC::TC("qpdf", "QPDFJob bytes fallback warning");
                        *m->log->getError()
                            << m->message_prefix
                            << ": WARNING: supplied password looks like a Unicode password with "
                               "characters not allowed in passwords for 40-bit and 128-bit "
                               "encryption; most readers will not be able to open this file with "
                               "the supplied password. (Use --password-mode=bytes to suppress this "
                               "warning and use the password anyway.)\n";
                    }
                } else if ((R >= 5) && (!is_valid_utf8)) {
                    QTC::TC("qpdf", "QPDFJob invalid utf-8 in auto");
                    throw std::runtime_error(
                        "supplied password is not a valid Unicode password, which is required for "
                        "256-bit encryption; to really use this password, rerun with the "
                        "--password-mode=bytes option");
                }
            }
        }
        break;
    }
}

void
QPDFJob::setEncryptionOptions(QPDFWriter& w)
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
        *m->log->getError() << m->message_prefix << ": -accessibility=n is ignored for modern"
                            << " encryption formats\n";
    }
    maybeFixWritePassword(R, m->user_password);
    maybeFixWritePassword(R, m->owner_password);
    if ((R < 4) || ((R == 4) && (!m->use_aes))) {
        if (!m->allow_weak_crypto) {
            QTC::TC("qpdf", "QPDFJob weak crypto error");
            *m->log->getError()
                << m->message_prefix
                << ": refusing to write a file with RC4, a weak cryptographic algorithm\n"
                   "Please use 256-bit keys for better security.\n"
                   "Pass --allow-weak-crypto to enable writing insecure files.\n"
                   "See also https://qpdf.readthedocs.io/en/stable/weak-crypto.html\n";
            throw std::runtime_error("refusing to write a file with weak crypto");
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
parse_version(std::string const& full_version_string, std::string& version, int& extension_level)
{
    auto vp = QUtil::make_unique_cstr(full_version_string);
    char* v = vp.get();
    char* p1 = strchr(v, '.');
    char* p2 = (p1 ? strchr(1 + p1, '.') : nullptr);
    if (p2 && *(p2 + 1)) {
        *p2++ = '\0';
        extension_level = QUtil::string_to_int(p2);
    }
    version = v;
}

void
QPDFJob::setWriterOptions(QPDFWriter& w)
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
        std::unique_ptr<QPDF> encryption_pdf;
        processFile(
            encryption_pdf,
            m->encryption_file.c_str(),
            m->encryption_file_password.get(),
            false,
            false);
        w.copyEncryptionParameters(*encryption_pdf);
    }
    if (m->encrypt) {
        setEncryptionOptions(w);
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
    w.setMinimumPDFVersion(m->max_input_version);
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
    if (m->progress) {
        if (m->progress_handler) {
            w.registerProgressReporter(
                std::shared_ptr<QPDFWriter::ProgressReporter>(
                    new QPDFWriter::FunctionProgressReporter(m->progress_handler)));
        } else {
            char const* outfilename = m->outfilename ? m->outfilename.get() : "standard output";
            w.registerProgressReporter(
                std::shared_ptr<QPDFWriter::ProgressReporter>(
                    // line-break
                    new ProgressReporter(*m->log->getInfo(), m->message_prefix, outfilename)));
        }
    }
}

void
QPDFJob::doSplitPages(QPDF& pdf)
{
    // Generate output file pattern
    std::string before;
    std::string after;
    size_t len = strlen(m->outfilename.get());
    char* num_spot = strstr(const_cast<char*>(m->outfilename.get()), "%d");
    if (num_spot != nullptr) {
        QTC::TC("qpdf", "QPDFJob split-pages %d");
        before = std::string(m->outfilename.get(), QIntC::to_size(num_spot - m->outfilename.get()));
        after = num_spot + 2;
    } else if (
        (len >= 4) && (QUtil::str_compare_nocase(m->outfilename.get() + len - 4, ".pdf") == 0)) {
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
    size_t pageno_len = std::to_string(pages.size()).length();
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
                        ("Exception caught while fixing copied annotations. This may be a qpdf "
                         "bug." +
                         std::string("Exception: ") + e.what()));
                }
            }
        }
        if (pldh.hasPageLabels()) {
            std::vector<QPDFObjectHandle> labels;
            pldh.getLabelsForPageRange(
                QIntC::to_longlong(first - 1), QIntC::to_longlong(last - 1), 0, labels);
            QPDFObjectHandle page_labels = QPDFObjectHandle::newDictionary();
            page_labels.replaceKey("/Nums", QPDFObjectHandle::newArray(labels));
            outpdf.getRoot().replaceKey("/PageLabels", page_labels);
        }
        std::string page_range = QUtil::uint_to_string(first, QIntC::to_int(pageno_len));
        if (m->split_pages > 1) {
            page_range += "-" + QUtil::uint_to_string(last, QIntC::to_int(pageno_len));
        }
        std::string outfile = before + page_range + after;
        if (QUtil::same_file(m->infilename.get(), outfile.c_str())) {
            throw std::runtime_error("split pages would overwrite input file with " + outfile);
        }
        QPDFWriter w(outpdf, outfile.c_str());
        setWriterOptions(w);
        w.write();
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": wrote file " << outfile << "\n";
        });
    }
}

void
QPDFJob::writeOutfile(QPDF& pdf)
{
    std::shared_ptr<char> temp_out;
    if (m->replace_input) {
        // Append but don't prepend to the path to generate a temporary name. This saves us from
        // having to split the path by directory and non-directory.
        temp_out = QUtil::make_shared_cstr(std::string(m->infilename.get()) + ".~qpdf-temp#");
        // m->outfilename will be restored to 0 before temp_out goes out of scope.
        m->outfilename = temp_out;
    } else if (strcmp(m->outfilename.get(), "-") == 0) {
        m->outfilename = nullptr;
    }
    if (m->json_version) {
        writeJSON(pdf);
    } else {
        // QPDFWriter must have block scope so the output file will be closed after write()
        // finishes.
        QPDFWriter w(pdf);
        if (m->outfilename) {
            w.setOutputFilename(m->outfilename.get());
        } else {
            // saveToStandardOutput has already been called, but calling it again is defensive and
            // harmless.
            m->log->saveToStandardOutput(true);
            w.setOutputPipeline(m->log->getSave().get());
        }
        setWriterOptions(w);
        w.write();
    }
    if (m->outfilename) {
        doIfVerbose([&](Pipeline& v, std::string const& prefix) {
            v << prefix << ": wrote file " << m->outfilename.get() << "\n";
        });
    }
    if (m->replace_input) {
        m->outfilename = nullptr;
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
            *m->log->getError() << m->message_prefix
                                << ": there are warnings; original file kept in " << backup << "\n";
        } else {
            try {
                QUtil::remove_file(backup.c_str());
            } catch (QPDFSystemError& e) {
                *m->log->getError() << m->message_prefix << ": unable to delete original file ("
                                    << e.what() << ");" << " original file left in " << backup
                                    << ", but the input was successfully replaced\n";
            }
        }
    }
}

void
QPDFJob::writeJSON(QPDF& pdf)
{
    // File pipeline must have block scope so it will be closed after write.
    std::shared_ptr<QUtil::FileCloser> fc;
    std::shared_ptr<Pipeline> fp;
    if (m->outfilename.get()) {
        QTC::TC("qpdf", "QPDFJob write json to file");
        if (m->json_stream_prefix.empty()) {
            m->json_stream_prefix = m->outfilename.get();
        }
        fc = std::make_shared<QUtil::FileCloser>(QUtil::safe_fopen(m->outfilename.get(), "w"));
        fp = std::make_shared<Pl_StdioFile>("json output", fc->f);
    } else if ((m->json_stream_data == qpdf_sj_file) && m->json_stream_prefix.empty()) {
        QTC::TC("qpdf", "QPDFJob need json-stream-prefix for stdout");
        usage("please specify --json-stream-prefix since the input file name is unknown");
    } else {
        QTC::TC("qpdf", "QPDFJob write json to stdout");
        m->log->saveToStandardOutput(true);
        fp = m->log->getSave();
    }
    doJSON(pdf, fp.get());
}

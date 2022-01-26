#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <cstring>

static void usage(std::string const& msg)
{
    throw QPDFJob::ConfigError(msg);
}

QPDFJob::Config&
QPDFJob::Config::allowWeakCrypto()
{
    o.allow_weak_crypto = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::check()
{
    o.check = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::checkLinearization()
{
    o.check_linearization = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::coalesceContents()
{
    o.coalesce_contents = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::collate(char const* parameter)
{
    auto n = ((parameter == 0) ? 1 :
              QUtil::string_to_uint(parameter));
    o.collate = QIntC::to_size(n);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::compressStreams(char const* parameter)
{
    o.compress_streams_set = true;
    o.compress_streams = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::compressionLevel(char const* parameter)
{
    o.compression_level = QUtil::string_to_int(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::copyEncryption(char const* parameter)
{
    o.encryption_file = parameter;
    o.copy_encryption = true;
    o.encrypt = false;
    o.decrypt = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::decrypt()
{
    o.decrypt = true;
    o.encrypt = false;
    o.copy_encryption = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::deterministicId()
{
    o.deterministic_id = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::empty()
{
    o.infilename = QUtil::make_shared_cstr("");
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::encryptionFilePassword(char const* parameter)
{
    o.encryption_file_password = QUtil::make_shared_cstr(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::externalizeInlineImages()
{
    o.externalize_inline_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::filteredStreamData()
{
    o.show_filtered_stream_data = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::flattenAnnotations(char const* parameter)
{
    o.flatten_annotations = true;
    if (strcmp(parameter, "screen") == 0)
    {
        o.flatten_annotations_forbidden |= an_no_view;
    }
    else if (strcmp(parameter, "print") == 0)
    {
        o.flatten_annotations_required |= an_print;
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::flattenRotation()
{
    o.flatten_rotation = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::forceVersion(char const* parameter)
{
    o.force_version = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::generateAppearances()
{
    o.generate_appearances = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::ignoreXrefStreams()
{
    o.ignore_xref_streams = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::iiMinBytes(char const* parameter)
{
    o.ii_min_bytes = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::isEncrypted()
{
    o.check_is_encrypted = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::json()
{
    o.json = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::jsonKey(char const* parameter)
{
    o.json_keys.insert(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::jsonObject(char const* parameter)
{
    o.json_objects.insert(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::keepFilesOpen(char const* parameter)
{
    o.keep_files_open_set = true;
    o.keep_files_open = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::keepFilesOpenThreshold(char const* parameter)
{
    o.keep_files_open_threshold = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::keepInlineImages()
{
    o.keep_inline_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::linearize()
{
    o.linearize = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::linearizePass1(char const* parameter)
{
    o.linearize_pass1 = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::listAttachments()
{
    o.list_attachments = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::minVersion(char const* parameter)
{
    o.min_version = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::newlineBeforeEndstream()
{
    o.newline_before_endstream = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::noOriginalObjectIds()
{
    o.suppress_original_object_id = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::noWarn()
{
    o.suppress_warnings = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::normalizeContent(char const* parameter)
{
    o.normalize_set = true;
    o.normalize = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::oiMinArea(char const* parameter)
{
    o.oi_min_area = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::oiMinHeight(char const* parameter)
{
    o.oi_min_height = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::oiMinWidth(char const* parameter)
{
    o.oi_min_width = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::optimizeImages()
{
    o.optimize_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::password(char const* parameter)
{
    o.password = QUtil::make_shared_cstr(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::passwordIsHexKey()
{
    o.password_is_hex_key = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::preserveUnreferenced()
{
    o.preserve_unreferenced_objects = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::preserveUnreferencedResources()
{
    o.remove_unreferenced_page_resources = QPDFJob::re_no;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::progress()
{
    o.progress = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::qdf()
{
    o.qdf_mode = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::rawStreamData()
{
    o.show_raw_stream_data = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::recompressFlate()
{
    o.recompress_flate_set = true;
    o.recompress_flate = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::removeAttachment(char const* parameter)
{
    o.attachments_to_remove.push_back(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::removePageLabels()
{
    o.remove_page_labels = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::replaceInput()
{
    o.replace_input = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::requiresPassword()
{
    o.check_requires_password = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showAttachment(char const* parameter)
{
    o.attachment_to_show = parameter;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showEncryption()
{
    o.show_encryption = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showEncryptionKey()
{
    o.show_encryption_key = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showLinearization()
{
    o.show_linearization = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showNpages()
{
    o.show_npages = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showPages()
{
    o.show_pages = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showXref()
{
    o.show_xref = true;
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::splitPages(char const* parameter)
{
    int n = ((parameter == 0) ? 1 :
             QUtil::string_to_int(parameter));
    o.split_pages = n;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::staticAesIv()
{
    o.static_aes_iv = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::staticId()
{
    o.static_id = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::suppressPasswordRecovery()
{
    o.suppress_password_recovery = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::suppressRecovery()
{
    o.suppress_recovery = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::verbose()
{
    o.m->verbose = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::warningExit0()
{
    o.warnings_exit_zero = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::withImages()
{
    o.show_page_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::passwordFile(char const* parameter)
{
    std::list<std::string> lines;
    if (strcmp(parameter, "-") == 0)
    {
        QTC::TC("qpdf", "QPDFJob_config password stdin");
        lines = QUtil::read_lines_from_file(std::cin);
    }
    else
    {
        QTC::TC("qpdf", "QPDFJob_config password file");
        lines = QUtil::read_lines_from_file(parameter);
    }
    if (lines.size() >= 1)
    {
        o.password = QUtil::make_shared_cstr(lines.front());

        if (lines.size() > 1)
        {
            std::cerr << this->o.m->message_prefix
                      << ": WARNING: all but the first line of"
                      << " the password file are ignored" << std::endl;
        }
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::passwordMode(char const* parameter)
{
    if (strcmp(parameter, "bytes") == 0)
    {
        o.password_mode = QPDFJob::pm_bytes;
    }
    else if (strcmp(parameter, "hex-bytes") == 0)
    {
        o.password_mode = QPDFJob::pm_hex_bytes;
    }
    else if (strcmp(parameter, "unicode") == 0)
    {
        o.password_mode = QPDFJob::pm_unicode;
    }
    else if (strcmp(parameter, "auto") == 0)
    {
        o.password_mode = QPDFJob::pm_auto;
    }
    else
    {
        usage("invalid password-mode option");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::streamData(char const* parameter)
{
    o.stream_data_set = true;
    if (strcmp(parameter, "compress") == 0)
    {
        o.stream_data_mode = qpdf_s_compress;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.stream_data_mode = qpdf_s_preserve;
    }
    else if (strcmp(parameter, "uncompress") == 0)
    {
        o.stream_data_mode = qpdf_s_uncompress;
    }
    else
    {
        // If this happens, it means streamDataChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid stream-data option");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::decodeLevel(char const* parameter)
{
    o.decode_level_set = true;
    if (strcmp(parameter, "none") == 0)
    {
        o.decode_level = qpdf_dl_none;
    }
    else if (strcmp(parameter, "generalized") == 0)
    {
        o.decode_level = qpdf_dl_generalized;
    }
    else if (strcmp(parameter, "specialized") == 0)
    {
        o.decode_level = qpdf_dl_specialized;
    }
    else if (strcmp(parameter, "all") == 0)
    {
        o.decode_level = qpdf_dl_all;
    }
    else
    {
        // If this happens, it means decodeLevelChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid option");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::objectStreams(char const* parameter)
{
    o.object_stream_set = true;
    if (strcmp(parameter, "disable") == 0)
    {
        o.object_stream_mode = qpdf_o_disable;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.object_stream_mode = qpdf_o_preserve;
    }
    else if (strcmp(parameter, "generate") == 0)
    {
        o.object_stream_mode = qpdf_o_generate;
    }
    else
    {
        // If this happens, it means objectStreamsChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid object stream mode");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::removeUnreferencedResources(char const* parameter)
{
    if (strcmp(parameter, "auto") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_auto;
    }
    else if (strcmp(parameter, "yes") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_yes;
    }
    else if (strcmp(parameter, "no") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_no;
    }
    else
    {
        // If this happens, it means remove_unref_choices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid value for --remove-unreferenced-page-resources");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showObject(char const* parameter)
{
    QPDFJob::parse_object_id(parameter, o.show_trailer, o.show_obj, o.show_gen);
    o.require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::jobJsonFile(char const* parameter)
{
    PointerHolder<char> file_buf;
    size_t size;
    QUtil::read_file_into_memory(parameter, file_buf, size);
    try
    {
        o.initializeFromJson(std::string(file_buf.getPointer(), size));
    }
    catch (std::exception& e)
    {
        throw std::runtime_error(
            "error with job-json file " + std::string(parameter) + " " +
            e.what() + "\nRun " + this->o.m->message_prefix +
            "--job-json-help for information on the file format.");
    }
    return *this;
}

std::shared_ptr<QPDFJob::CopyAttConfig>
QPDFJob::Config::copyAttachmentsFrom()
{
    return std::shared_ptr<CopyAttConfig>(new CopyAttConfig(*this));
}

QPDFJob::CopyAttConfig::CopyAttConfig(Config& c) :
    config(c)
{
}

QPDFJob::CopyAttConfig&
QPDFJob::CopyAttConfig::path(char const* parameter)
{
    this->caf.path = parameter;
    return *this;
}

QPDFJob::CopyAttConfig&
QPDFJob::CopyAttConfig::prefix(char const* parameter)
{
    this->caf.prefix = parameter;
    return *this;
}

QPDFJob::CopyAttConfig&
QPDFJob::CopyAttConfig::password(char const* parameter)
{
    this->caf.password = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::CopyAttConfig::end()
{
    if (this->caf.path.empty())
    {
        usage("copy attachments: no path specified");
    }
    this->config.o.attachments_to_copy.push_back(this->caf);
    return this->config;
}

QPDFJob::AttConfig::AttConfig(Config& c) :
    config(c)
{
}

std::shared_ptr<QPDFJob::AttConfig>
QPDFJob::Config::addAttachment()
{
    return std::shared_ptr<AttConfig>(new AttConfig(*this));
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::path(char const* parameter)
{
    this->att.path = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::key(char const* parameter)
{
    this->att.key = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::filename(char const* parameter)
{
    this->att.filename = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::creationdate(char const* parameter)
{
    if (! QUtil::pdf_time_to_qpdf_time(parameter))
    {
        usage(std::string(parameter) + " is not a valid PDF timestamp");
    }
    this->att.creationdate = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::moddate(char const* parameter)
{
    if (! QUtil::pdf_time_to_qpdf_time(parameter))
    {
        usage(std::string(parameter) + " is not a valid PDF timestamp");
    }
    this->att.moddate = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::mimetype(char const* parameter)
{
    if (strchr(parameter, '/') == nullptr)
    {
        usage("mime type should be specified as type/subtype");
    }
    this->att.mimetype = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::description(char const* parameter)
{
    this->att.description = parameter;
    return *this;
}

QPDFJob::AttConfig&
QPDFJob::AttConfig::replace()
{
    this->att.replace = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::AttConfig::end()
{
    static std::string now = QUtil::qpdf_time_to_pdf_time(
        QUtil::get_current_qpdf_time());
    if (this->att.path.empty())
    {
        usage("add attachment: no path specified");
    }
    std::string last_element = QUtil::path_basename(this->att.path);
    if (last_element.empty())
    {
        usage("path for --add-attachment may not be empty");
    }
    if (this->att.filename.empty())
    {
        this->att.filename = last_element;
    }
    if (this->att.key.empty())
    {
        this->att.key = last_element;
    }
    if (this->att.creationdate.empty())
    {
        this->att.creationdate = now;
    }
    if (this->att.moddate.empty())
    {
        this->att.moddate = now;
    }

    this->config.o.attachments_to_add.push_back(this->att);
    return this->config;
}

QPDFJob::PagesConfig::PagesConfig(Config& c) :
    config(c)
{
}

std::shared_ptr<QPDFJob::PagesConfig>
QPDFJob::Config::pages()
{
    return std::shared_ptr<PagesConfig>(new PagesConfig(*this));
}

QPDFJob::Config&
QPDFJob::PagesConfig::end()
{
    if (this->config.o.page_specs.empty())
    {
        usage("--pages: no page specifications given");
    }
    return this->config;
}

QPDFJob::PagesConfig&
QPDFJob::PagesConfig::pageSpec(std::string const& filename,
                               char const* password,
                               std::string const& range)
{
    this->config.o.page_specs.push_back(
        QPDFJob::PageSpec(filename, password, range));
    return *this;
}

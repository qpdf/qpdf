#include <qpdf/QPDFJob.hh>

#include <qpdf/QPDFLogger.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

void
QPDFJob::Config::checkConfiguration()
{
    o.checkConfiguration();
}

QPDFJob::Config*
QPDFJob::Config::inputFile(std::string const& filename)
{
    if (o.m->infilename == nullptr) {
        o.m->infilename = QUtil::make_shared_cstr(filename);
    } else {
        usage("input file has already been given");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::emptyInput()
{
    if (o.m->infilename == nullptr) {
        // Various places in QPDFJob.cc know that the empty string for
        // infile means empty. We set it to something other than a
        // null pointer as an indication that some input source has
        // been specified. This approach means that passing "" as
        // the argument to inputFile in job JSON, or equivalently
        // using "" as a positional command-line argument would be the
        // same as --empty. This probably isn't worth blocking or
        // coding around.
        o.m->infilename = QUtil::make_shared_cstr("");
    } else {
        usage("empty input can't be used"
              " since input file has already been given");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::outputFile(std::string const& filename)
{
    if ((o.m->outfilename == nullptr) && (!o.m->replace_input)) {
        o.m->outfilename = QUtil::make_shared_cstr(filename);
    } else {
        usage("output file has already been given");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::replaceInput()
{
    if ((o.m->outfilename == nullptr) && (!o.m->replace_input)) {
        o.m->replace_input = true;
    } else {
        usage("replace-input can't be used"
              " since output file has already been given");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::allowWeakCrypto()
{
    o.m->allow_weak_crypto = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::check()
{
    o.m->check = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::checkLinearization()
{
    o.m->check_linearization = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::coalesceContents()
{
    o.m->coalesce_contents = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::collate()
{
    return collate("");
}

QPDFJob::Config*
QPDFJob::Config::collate(std::string const& parameter)
{
    auto n = (parameter.empty() ? 1 : QUtil::string_to_uint(parameter.c_str()));
    o.m->collate = QIntC::to_size(n);
    return this;
}

QPDFJob::Config*
QPDFJob::Config::compressStreams(std::string const& parameter)
{
    o.m->compress_streams_set = true;
    o.m->compress_streams = (parameter == "y");
    return this;
}

QPDFJob::Config*
QPDFJob::Config::compressionLevel(std::string const& parameter)
{
    o.m->compression_level = QUtil::string_to_int(parameter.c_str());
    return this;
}

QPDFJob::Config*
QPDFJob::Config::copyEncryption(std::string const& parameter)
{
    o.m->encryption_file = parameter;
    o.m->copy_encryption = true;
    o.m->encrypt = false;
    o.m->decrypt = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::decrypt()
{
    o.m->decrypt = true;
    o.m->encrypt = false;
    o.m->copy_encryption = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::deterministicId()
{
    o.m->deterministic_id = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::encryptionFilePassword(std::string const& parameter)
{
    o.m->encryption_file_password = QUtil::make_shared_cstr(parameter);
    return this;
}

QPDFJob::Config*
QPDFJob::Config::externalizeInlineImages()
{
    o.m->externalize_inline_images = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::filteredStreamData()
{
    o.m->show_filtered_stream_data = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::flattenAnnotations(std::string const& parameter)
{
    o.m->flatten_annotations = true;
    if (parameter == "screen") {
        o.m->flatten_annotations_forbidden |= an_no_view;
    } else if (parameter == "print") {
        o.m->flatten_annotations_required |= an_print;
    } else if (parameter != "all") {
        usage("invalid flatten-annotations option");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::flattenRotation()
{
    o.m->flatten_rotation = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::forceVersion(std::string const& parameter)
{
    o.m->force_version = parameter;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::generateAppearances()
{
    o.m->generate_appearances = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::ignoreXrefStreams()
{
    o.m->ignore_xref_streams = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::iiMinBytes(std::string const& parameter)
{
    o.m->ii_min_bytes = QUtil::string_to_uint(parameter.c_str());
    return this;
}

QPDFJob::Config*
QPDFJob::Config::isEncrypted()
{
    o.m->check_is_encrypted = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::json()
{
    return json("");
}

QPDFJob::Config*
QPDFJob::Config::json(std::string const& parameter)
{
    if (parameter.empty() || (parameter == "latest")) {
        o.m->json_version = JSON::LATEST;
    } else {
        o.m->json_version = QUtil::string_to_int(parameter.c_str());
    }
    if ((o.m->json_version < 1) || (o.m->json_version > JSON::LATEST)) {
        usage(std::string("unsupported json version ") + parameter);
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::jsonKey(std::string const& parameter)
{
    o.m->json_keys.insert(parameter);
    return this;
}

QPDFJob::Config*
QPDFJob::Config::jsonObject(std::string const& parameter)
{
    o.m->json_objects.insert(parameter);
    return this;
}

QPDFJob::Config*
QPDFJob::Config::jsonStreamData(std::string const& parameter)
{
    o.m->json_stream_data_set = true;
    if (parameter == "none") {
        o.m->json_stream_data = qpdf_sj_none;
    } else if (parameter == "inline") {
        o.m->json_stream_data = qpdf_sj_inline;
    } else if (parameter == "file") {
        o.m->json_stream_data = qpdf_sj_file;
    } else {
        usage("invalid json-streams option");
    }

    return this;
}

QPDFJob::Config*
QPDFJob::Config::jsonStreamPrefix(std::string const& parameter)
{
    o.m->json_stream_prefix = parameter;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::jsonInput()
{
    o.m->json_input = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::jsonOutput(std::string const& parameter)
{
    o.m->json_output = true;
    json(parameter);
    if (!o.m->json_stream_data_set) {
        // No need to set json_stream_data_set -- that indicates
        // explicit use of --json-stream-data.
        o.m->json_stream_data = qpdf_sj_inline;
    }
    if (!o.m->decode_level_set) {
        o.m->decode_level = qpdf_dl_none;
    }
    o.m->json_keys.insert("qpdf");
    return this;
}

QPDFJob::Config*
QPDFJob::Config::updateFromJson(std::string const& parameter)
{
    o.m->update_from_json = parameter;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::testJsonSchema()
{
    o.m->test_json_schema = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::keepFilesOpen(std::string const& parameter)
{
    o.m->keep_files_open_set = true;
    o.m->keep_files_open = (parameter == "y");
    return this;
}

QPDFJob::Config*
QPDFJob::Config::keepFilesOpenThreshold(std::string const& parameter)
{
    o.m->keep_files_open_threshold = QUtil::string_to_uint(parameter.c_str());
    return this;
}

QPDFJob::Config*
QPDFJob::Config::keepInlineImages()
{
    o.m->keep_inline_images = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::linearize()
{
    o.m->linearize = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::linearizePass1(std::string const& parameter)
{
    o.m->linearize_pass1 = parameter;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::listAttachments()
{
    o.m->list_attachments = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::minVersion(std::string const& parameter)
{
    o.m->min_version = parameter;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::newlineBeforeEndstream()
{
    o.m->newline_before_endstream = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::noOriginalObjectIds()
{
    o.m->suppress_original_object_id = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::noWarn()
{
    o.m->suppress_warnings = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::normalizeContent(std::string const& parameter)
{
    o.m->normalize_set = true;
    o.m->normalize = (parameter == "y");
    return this;
}

QPDFJob::Config*
QPDFJob::Config::oiMinArea(std::string const& parameter)
{
    o.m->oi_min_area = QUtil::string_to_uint(parameter.c_str());
    return this;
}

QPDFJob::Config*
QPDFJob::Config::oiMinHeight(std::string const& parameter)
{
    o.m->oi_min_height = QUtil::string_to_uint(parameter.c_str());
    return this;
}

QPDFJob::Config*
QPDFJob::Config::oiMinWidth(std::string const& parameter)
{
    o.m->oi_min_width = QUtil::string_to_uint(parameter.c_str());
    return this;
}

QPDFJob::Config*
QPDFJob::Config::optimizeImages()
{
    o.m->optimize_images = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::password(std::string const& parameter)
{
    o.m->password = QUtil::make_shared_cstr(parameter);
    return this;
}

QPDFJob::Config*
QPDFJob::Config::passwordIsHexKey()
{
    o.m->password_is_hex_key = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::preserveUnreferenced()
{
    o.m->preserve_unreferenced_objects = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::preserveUnreferencedResources()
{
    o.m->remove_unreferenced_page_resources = QPDFJob::re_no;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::progress()
{
    o.m->progress = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::qdf()
{
    o.m->qdf_mode = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::rawStreamData()
{
    o.m->show_raw_stream_data = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::recompressFlate()
{
    o.m->recompress_flate_set = true;
    o.m->recompress_flate = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::removeAttachment(std::string const& parameter)
{
    o.m->attachments_to_remove.push_back(parameter);
    return this;
}

QPDFJob::Config*
QPDFJob::Config::removePageLabels()
{
    o.m->remove_page_labels = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::reportMemoryUsage()
{
    o.m->report_mem_usage = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::requiresPassword()
{
    o.m->check_requires_password = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::removeRestrictions()
{
    o.m->remove_restrictions = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showAttachment(std::string const& parameter)
{
    o.m->attachment_to_show = parameter;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showEncryption()
{
    o.m->show_encryption = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showEncryptionKey()
{
    o.m->show_encryption_key = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showLinearization()
{
    o.m->show_linearization = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showNpages()
{
    o.m->show_npages = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showPages()
{
    o.m->show_pages = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showXref()
{
    o.m->show_xref = true;
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::splitPages()
{
    return splitPages("");
}

QPDFJob::Config*
QPDFJob::Config::splitPages(std::string const& parameter)
{
    int n = (parameter.empty() ? 1 : QUtil::string_to_int(parameter.c_str()));
    o.m->split_pages = n;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::staticAesIv()
{
    o.m->static_aes_iv = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::staticId()
{
    o.m->static_id = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::suppressPasswordRecovery()
{
    o.m->suppress_password_recovery = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::suppressRecovery()
{
    o.m->suppress_recovery = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::verbose()
{
    o.m->verbose = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::warningExit0()
{
    o.m->warnings_exit_zero = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::withImages()
{
    o.m->show_page_images = true;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::passwordFile(std::string const& parameter)
{
    std::list<std::string> lines;
    if (parameter == "-") {
        QTC::TC("qpdf", "QPDFJob_config password stdin");
        lines = QUtil::read_lines_from_file(std::cin);
    } else {
        QTC::TC("qpdf", "QPDFJob_config password file");
        lines = QUtil::read_lines_from_file(parameter.c_str());
    }
    if (lines.size() >= 1) {
        o.m->password = QUtil::make_shared_cstr(lines.front());

        if (lines.size() > 1) {
            *QPDFLogger::defaultLogger()->getError()
                << this->o.m->message_prefix
                << ": WARNING: all but the first line of"
                << " the password file are ignored\n";
        }
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::passwordMode(std::string const& parameter)
{
    if (parameter == "bytes") {
        o.m->password_mode = QPDFJob::pm_bytes;
    } else if (parameter == "hex-bytes") {
        o.m->password_mode = QPDFJob::pm_hex_bytes;
    } else if (parameter == "unicode") {
        o.m->password_mode = QPDFJob::pm_unicode;
    } else if (parameter == "auto") {
        o.m->password_mode = QPDFJob::pm_auto;
    } else {
        usage("invalid password-mode option");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::streamData(std::string const& parameter)
{
    o.m->stream_data_set = true;
    if (parameter == "compress") {
        o.m->stream_data_mode = qpdf_s_compress;
    } else if (parameter == "preserve") {
        o.m->stream_data_mode = qpdf_s_preserve;
    } else if (parameter == "uncompress") {
        o.m->stream_data_mode = qpdf_s_uncompress;
    } else {
        usage("invalid stream-data option");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::decodeLevel(std::string const& parameter)
{
    o.m->decode_level_set = true;
    if (parameter == "none") {
        o.m->decode_level = qpdf_dl_none;
    } else if (parameter == "generalized") {
        o.m->decode_level = qpdf_dl_generalized;
    } else if (parameter == "specialized") {
        o.m->decode_level = qpdf_dl_specialized;
    } else if (parameter == "all") {
        o.m->decode_level = qpdf_dl_all;
    } else {
        usage("invalid option");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::objectStreams(std::string const& parameter)
{
    o.m->object_stream_set = true;
    if (parameter == "disable") {
        o.m->object_stream_mode = qpdf_o_disable;
    } else if (parameter == "preserve") {
        o.m->object_stream_mode = qpdf_o_preserve;
    } else if (parameter == "generate") {
        o.m->object_stream_mode = qpdf_o_generate;
    } else {
        usage("invalid object stream mode");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::removeUnreferencedResources(std::string const& parameter)
{
    if (parameter == "auto") {
        o.m->remove_unreferenced_page_resources = QPDFJob::re_auto;
    } else if (parameter == "yes") {
        o.m->remove_unreferenced_page_resources = QPDFJob::re_yes;
    } else if (parameter == "no") {
        o.m->remove_unreferenced_page_resources = QPDFJob::re_no;
    } else {
        usage("invalid value for --remove-unreferenced-page-resources");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::showObject(std::string const& parameter)
{
    QPDFJob::parse_object_id(
        parameter, o.m->show_trailer, o.m->show_obj, o.m->show_gen);
    o.m->require_outfile = false;
    return this;
}

QPDFJob::Config*
QPDFJob::Config::jobJsonFile(std::string const& parameter)
{
    std::shared_ptr<char> file_buf;
    size_t size;
    QUtil::read_file_into_memory(parameter.c_str(), file_buf, size);
    try {
        o.initializeFromJson(std::string(file_buf.get(), size), true);
    } catch (std::exception& e) {
        throw std::runtime_error(
            "error with job-json file " + std::string(parameter) + ": " +
            e.what() + "\nRun " + this->o.m->message_prefix +
            " --job-json-help for information on the file format.");
    }
    return this;
}

QPDFJob::Config*
QPDFJob::Config::rotate(std::string const& parameter)
{
    o.parseRotationParameter(parameter);
    return this;
}

std::shared_ptr<QPDFJob::CopyAttConfig>
QPDFJob::Config::copyAttachmentsFrom()
{
    return std::shared_ptr<CopyAttConfig>(new CopyAttConfig(this));
}

QPDFJob::CopyAttConfig::CopyAttConfig(Config* c) :
    config(c)
{
}

QPDFJob::CopyAttConfig*
QPDFJob::CopyAttConfig::file(std::string const& parameter)
{
    this->caf.path = parameter;
    return this;
}

QPDFJob::CopyAttConfig*
QPDFJob::CopyAttConfig::prefix(std::string const& parameter)
{
    this->caf.prefix = parameter;
    return this;
}

QPDFJob::CopyAttConfig*
QPDFJob::CopyAttConfig::password(std::string const& parameter)
{
    this->caf.password = parameter;
    return this;
}

QPDFJob::Config*
QPDFJob::CopyAttConfig::endCopyAttachmentsFrom()
{
    if (this->caf.path.empty()) {
        usage("copy attachments: no file specified");
    }
    this->config->o.m->attachments_to_copy.push_back(this->caf);
    return this->config;
}

QPDFJob::AttConfig::AttConfig(Config* c) :
    config(c)
{
}

std::shared_ptr<QPDFJob::AttConfig>
QPDFJob::Config::addAttachment()
{
    return std::shared_ptr<AttConfig>(new AttConfig(this));
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::file(std::string const& parameter)
{
    this->att.path = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::key(std::string const& parameter)
{
    this->att.key = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::filename(std::string const& parameter)
{
    this->att.filename = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::creationdate(std::string const& parameter)
{
    if (!QUtil::pdf_time_to_qpdf_time(parameter)) {
        usage(std::string(parameter) + " is not a valid PDF timestamp");
    }
    this->att.creationdate = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::moddate(std::string const& parameter)
{
    if (!QUtil::pdf_time_to_qpdf_time(parameter)) {
        usage(std::string(parameter) + " is not a valid PDF timestamp");
    }
    this->att.moddate = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::mimetype(std::string const& parameter)
{
    if (parameter.find('/') == std::string::npos) {
        usage("mime type should be specified as type/subtype");
    }
    this->att.mimetype = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::description(std::string const& parameter)
{
    this->att.description = parameter;
    return this;
}

QPDFJob::AttConfig*
QPDFJob::AttConfig::replace()
{
    this->att.replace = true;
    return this;
}

QPDFJob::Config*
QPDFJob::AttConfig::endAddAttachment()
{
    static std::string now =
        QUtil::qpdf_time_to_pdf_time(QUtil::get_current_qpdf_time());
    if (this->att.path.empty()) {
        usage("add attachment: no file specified");
    }
    std::string last_element = QUtil::path_basename(this->att.path);
    if (last_element.empty()) {
        usage("file for --add-attachment may not be empty");
    }
    if (this->att.filename.empty()) {
        this->att.filename = last_element;
    }
    if (this->att.key.empty()) {
        this->att.key = last_element;
    }
    if (this->att.creationdate.empty()) {
        this->att.creationdate = now;
    }
    if (this->att.moddate.empty()) {
        this->att.moddate = now;
    }

    this->config->o.m->attachments_to_add.push_back(this->att);
    return this->config;
}

QPDFJob::PagesConfig::PagesConfig(Config* c) :
    config(c)
{
}

std::shared_ptr<QPDFJob::PagesConfig>
QPDFJob::Config::pages()
{
    if (!o.m->page_specs.empty()) {
        usage("--pages may only be specified one time");
    }
    return std::shared_ptr<PagesConfig>(new PagesConfig(this));
}

QPDFJob::Config*
QPDFJob::PagesConfig::endPages()
{
    if (this->config->o.m->page_specs.empty()) {
        usage("--pages: no page specifications given");
    }
    return this->config;
}

QPDFJob::PagesConfig*
QPDFJob::PagesConfig::pageSpec(
    std::string const& filename, std::string const& range, char const* password)
{
    this->config->o.m->page_specs.push_back(
        QPDFJob::PageSpec(filename, password, range));
    return this;
}

std::shared_ptr<QPDFJob::UOConfig>
QPDFJob::Config::overlay()
{
    o.m->under_overlay = &o.m->overlay;
    return std::shared_ptr<UOConfig>(new UOConfig(this));
}

std::shared_ptr<QPDFJob::UOConfig>
QPDFJob::Config::underlay()
{
    o.m->under_overlay = &o.m->underlay;
    return std::shared_ptr<UOConfig>(new UOConfig(this));
}

QPDFJob::UOConfig::UOConfig(Config* c) :
    config(c)
{
}

QPDFJob::Config*
QPDFJob::UOConfig::endUnderlayOverlay()
{
    if (config->o.m->under_overlay->filename.empty()) {
        usage(config->o.m->under_overlay->which + " file not specified");
    }
    config->o.m->under_overlay = nullptr;
    return this->config;
}

QPDFJob::UOConfig*
QPDFJob::UOConfig::file(std::string const& parameter)
{
    if (!config->o.m->under_overlay->filename.empty()) {
        usage(config->o.m->under_overlay->which + " file already specified");
    } else {
        config->o.m->under_overlay->filename = parameter;
    }
    return this;
}

QPDFJob::UOConfig*
QPDFJob::UOConfig::to(std::string const& parameter)
{
    config->o.parseNumrange(parameter.c_str(), 0);
    config->o.m->under_overlay->to_nr = parameter;
    return this;
}

QPDFJob::UOConfig*
QPDFJob::UOConfig::from(std::string const& parameter)
{
    if (!parameter.empty()) {
        config->o.parseNumrange(parameter.c_str(), 0);
    }
    config->o.m->under_overlay->from_nr = parameter;
    return this;
}

QPDFJob::UOConfig*
QPDFJob::UOConfig::repeat(std::string const& parameter)
{
    if (!parameter.empty()) {
        config->o.parseNumrange(parameter.c_str(), 0);
    }
    config->o.m->under_overlay->repeat_nr = parameter;
    return this;
}

QPDFJob::UOConfig*
QPDFJob::UOConfig::password(std::string const& parameter)
{
    config->o.m->under_overlay->password = QUtil::make_shared_cstr(parameter);
    return this;
}

std::shared_ptr<QPDFJob::EncConfig>
QPDFJob::Config::encrypt(
    int keylen,
    std::string const& user_password,
    std::string const& owner_password)
{
    o.m->keylen = keylen;
    if (keylen == 256) {
        o.m->use_aes = true;
    }
    o.m->user_password = user_password;
    o.m->owner_password = owner_password;
    return std::shared_ptr<EncConfig>(new EncConfig(this));
}

QPDFJob::EncConfig::EncConfig(Config* c) :
    config(c)
{
}

QPDFJob::Config*
QPDFJob::EncConfig::endEncrypt()
{
    config->o.m->encrypt = true;
    config->o.m->decrypt = false;
    config->o.m->copy_encryption = false;
    return this->config;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::allowInsecure()
{
    config->o.m->allow_insecure = true;
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::accessibility(std::string const& parameter)
{
    config->o.m->r3_accessibility = (parameter == "y");
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::extract(std::string const& parameter)
{
    if (config->o.m->keylen == 40) {
        config->o.m->r2_extract = (parameter == "y");
    } else {
        config->o.m->r3_extract = (parameter == "y");
    }
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::print(std::string const& parameter)
{
    if (config->o.m->keylen == 40) {
        config->o.m->r2_print = (parameter == "y");
    } else if (parameter == "full") {
        config->o.m->r3_print = qpdf_r3p_full;
    } else if (parameter == "low") {
        config->o.m->r3_print = qpdf_r3p_low;
    } else if (parameter == "none") {
        config->o.m->r3_print = qpdf_r3p_none;
    } else {
        usage("invalid print option");
    }
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::modify(std::string const& parameter)
{
    if (config->o.m->keylen == 40) {
        config->o.m->r2_modify = (parameter == "y");
    } else if (parameter == "all") {
        config->o.m->r3_assemble = true;
        config->o.m->r3_annotate_and_form = true;
        config->o.m->r3_form_filling = true;
        config->o.m->r3_modify_other = true;
    } else if (parameter == "annotate") {
        config->o.m->r3_assemble = true;
        config->o.m->r3_annotate_and_form = true;
        config->o.m->r3_form_filling = true;
        config->o.m->r3_modify_other = false;
    } else if (parameter == "form") {
        config->o.m->r3_assemble = true;
        config->o.m->r3_annotate_and_form = false;
        config->o.m->r3_form_filling = true;
        config->o.m->r3_modify_other = false;
    } else if (parameter == "assembly") {
        config->o.m->r3_assemble = true;
        config->o.m->r3_annotate_and_form = false;
        config->o.m->r3_form_filling = false;
        config->o.m->r3_modify_other = false;
    } else if (parameter == "none") {
        config->o.m->r3_assemble = false;
        config->o.m->r3_annotate_and_form = false;
        config->o.m->r3_form_filling = false;
        config->o.m->r3_modify_other = false;
    } else {
        usage("invalid modify option");
    }
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::cleartextMetadata()
{
    config->o.m->cleartext_metadata = true;
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::assemble(std::string const& parameter)
{
    config->o.m->r3_assemble = (parameter == "y");
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::annotate(std::string const& parameter)
{
    if (config->o.m->keylen == 40) {
        config->o.m->r2_annotate = (parameter == "y");
    } else {
        config->o.m->r3_annotate_and_form = (parameter == "y");
    }
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::form(std::string const& parameter)
{
    config->o.m->r3_form_filling = (parameter == "y");
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::modifyOther(std::string const& parameter)
{
    config->o.m->r3_modify_other = (parameter == "y");
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::useAes(std::string const& parameter)
{
    config->o.m->use_aes = (parameter == "y");
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::forceV4()
{
    config->o.m->force_V4 = true;
    return this;
}

QPDFJob::EncConfig*
QPDFJob::EncConfig::forceR5()
{
    config->o.m->force_R5 = true;
    return this;
}

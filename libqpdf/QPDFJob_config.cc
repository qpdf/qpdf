#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <cstring>

static void usage(std::string const& msg)
{
    throw QPDFJob::ConfigError(msg);
}

QPDFJob::Config&
QPDFJob::Config::inputFile(char const* filename)
{
    if (o.m->infilename == 0)
    {
        o.m->infilename = QUtil::make_shared_cstr(filename);
    }
    else
    {
        usage("input file has already been given");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::emptyInput()
{
    if (o.m->infilename == 0)
    {
        // QXXXQ decide whether to fix this or just leave the comment:
        // Various places in QPDFJob.cc know that the empty string for
        // infile means empty. This means that passing "" as the
        // argument to inputFile, or equivalently using "" as a
        // positional command-line argument would be the same as
        // --empty. This probably isn't worth blocking or coding
        // around, but it would be better if we had a tighter way of
        // knowing that the input file is empty.
        o.m->infilename = QUtil::make_shared_cstr("");
    }
    else
    {
        usage("empty input can't be used"
              " since input file has already been given");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::outputFile(char const* filename)
{
    if ((o.m->outfilename == 0) && (! o.m->replace_input))
    {
        o.m->outfilename = QUtil::make_shared_cstr(filename);
    }
    else
    {
        usage("output file has already been given");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::replaceInput()
{
    if ((o.m->outfilename == 0) && (! o.m->replace_input))
    {
        o.m->replace_input = true;
    }
    else
    {
        usage("replace-input can't be used"
              " since output file has already been given");
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::allowWeakCrypto()
{
    o.m->allow_weak_crypto = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::check()
{
    o.m->check = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::checkLinearization()
{
    o.m->check_linearization = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::coalesceContents()
{
    o.m->coalesce_contents = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::collate(char const* parameter)
{
    auto n = ((parameter == 0) ? 1 :
              QUtil::string_to_uint(parameter));
    o.m->collate = QIntC::to_size(n);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::compressStreams(char const* parameter)
{
    o.m->compress_streams_set = true;
    o.m->compress_streams = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::compressionLevel(char const* parameter)
{
    o.m->compression_level = QUtil::string_to_int(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::copyEncryption(char const* parameter)
{
    o.m->encryption_file = parameter;
    o.m->copy_encryption = true;
    o.m->encrypt = false;
    o.m->decrypt = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::decrypt()
{
    o.m->decrypt = true;
    o.m->encrypt = false;
    o.m->copy_encryption = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::deterministicId()
{
    o.m->deterministic_id = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::encryptionFilePassword(char const* parameter)
{
    o.m->encryption_file_password = QUtil::make_shared_cstr(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::externalizeInlineImages()
{
    o.m->externalize_inline_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::filteredStreamData()
{
    o.m->show_filtered_stream_data = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::flattenAnnotations(char const* parameter)
{
    o.m->flatten_annotations = true;
    if (strcmp(parameter, "screen") == 0)
    {
        o.m->flatten_annotations_forbidden |= an_no_view;
    }
    else if (strcmp(parameter, "print") == 0)
    {
        o.m->flatten_annotations_required |= an_print;
    }
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::flattenRotation()
{
    o.m->flatten_rotation = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::forceVersion(char const* parameter)
{
    o.m->force_version = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::generateAppearances()
{
    o.m->generate_appearances = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::ignoreXrefStreams()
{
    o.m->ignore_xref_streams = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::iiMinBytes(char const* parameter)
{
    o.m->ii_min_bytes = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::isEncrypted()
{
    o.m->check_is_encrypted = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::json()
{
    o.m->json = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::jsonKey(char const* parameter)
{
    o.m->json_keys.insert(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::jsonObject(char const* parameter)
{
    o.m->json_objects.insert(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::keepFilesOpen(char const* parameter)
{
    o.m->keep_files_open_set = true;
    o.m->keep_files_open = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::keepFilesOpenThreshold(char const* parameter)
{
    o.m->keep_files_open_threshold = QUtil::string_to_uint(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::keepInlineImages()
{
    o.m->keep_inline_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::linearize()
{
    o.m->linearize = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::linearizePass1(char const* parameter)
{
    o.m->linearize_pass1 = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::listAttachments()
{
    o.m->list_attachments = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::minVersion(char const* parameter)
{
    o.m->min_version = parameter;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::newlineBeforeEndstream()
{
    o.m->newline_before_endstream = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::noOriginalObjectIds()
{
    o.m->suppress_original_object_id = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::noWarn()
{
    o.m->suppress_warnings = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::normalizeContent(char const* parameter)
{
    o.m->normalize_set = true;
    o.m->normalize = (strcmp(parameter, "y") == 0);
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
    o.m->optimize_images = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::password(char const* parameter)
{
    o.m->password = QUtil::make_shared_cstr(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::passwordIsHexKey()
{
    o.m->password_is_hex_key = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::preserveUnreferenced()
{
    o.m->preserve_unreferenced_objects = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::preserveUnreferencedResources()
{
    o.m->remove_unreferenced_page_resources = QPDFJob::re_no;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::progress()
{
    o.m->progress = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::qdf()
{
    o.m->qdf_mode = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::rawStreamData()
{
    o.m->show_raw_stream_data = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::recompressFlate()
{
    o.m->recompress_flate_set = true;
    o.m->recompress_flate = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::removeAttachment(char const* parameter)
{
    o.m->attachments_to_remove.push_back(parameter);
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::removePageLabels()
{
    o.m->remove_page_labels = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::requiresPassword()
{
    o.m->check_requires_password = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showAttachment(char const* parameter)
{
    o.m->attachment_to_show = parameter;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showEncryption()
{
    o.m->show_encryption = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showEncryptionKey()
{
    o.m->show_encryption_key = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showLinearization()
{
    o.m->show_linearization = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showNpages()
{
    o.m->show_npages = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showPages()
{
    o.m->show_pages = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::showXref()
{
    o.m->show_xref = true;
    o.m->require_outfile = false;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::splitPages(char const* parameter)
{
    int n = ((parameter == 0) ? 1 :
             QUtil::string_to_int(parameter));
    o.m->split_pages = n;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::staticAesIv()
{
    o.m->static_aes_iv = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::staticId()
{
    o.m->static_id = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::suppressPasswordRecovery()
{
    o.m->suppress_password_recovery = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::suppressRecovery()
{
    o.m->suppress_recovery = true;
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
    o.m->warnings_exit_zero = true;
    return *this;
}

QPDFJob::Config&
QPDFJob::Config::withImages()
{
    o.m->show_page_images = true;
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
        o.m->password = QUtil::make_shared_cstr(lines.front());

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
        o.m->password_mode = QPDFJob::pm_bytes;
    }
    else if (strcmp(parameter, "hex-bytes") == 0)
    {
        o.m->password_mode = QPDFJob::pm_hex_bytes;
    }
    else if (strcmp(parameter, "unicode") == 0)
    {
        o.m->password_mode = QPDFJob::pm_unicode;
    }
    else if (strcmp(parameter, "auto") == 0)
    {
        o.m->password_mode = QPDFJob::pm_auto;
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
    o.m->stream_data_set = true;
    if (strcmp(parameter, "compress") == 0)
    {
        o.m->stream_data_mode = qpdf_s_compress;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.m->stream_data_mode = qpdf_s_preserve;
    }
    else if (strcmp(parameter, "uncompress") == 0)
    {
        o.m->stream_data_mode = qpdf_s_uncompress;
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
    o.m->decode_level_set = true;
    if (strcmp(parameter, "none") == 0)
    {
        o.m->decode_level = qpdf_dl_none;
    }
    else if (strcmp(parameter, "generalized") == 0)
    {
        o.m->decode_level = qpdf_dl_generalized;
    }
    else if (strcmp(parameter, "specialized") == 0)
    {
        o.m->decode_level = qpdf_dl_specialized;
    }
    else if (strcmp(parameter, "all") == 0)
    {
        o.m->decode_level = qpdf_dl_all;
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
    o.m->object_stream_set = true;
    if (strcmp(parameter, "disable") == 0)
    {
        o.m->object_stream_mode = qpdf_o_disable;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.m->object_stream_mode = qpdf_o_preserve;
    }
    else if (strcmp(parameter, "generate") == 0)
    {
        o.m->object_stream_mode = qpdf_o_generate;
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
        o.m->remove_unreferenced_page_resources = QPDFJob::re_auto;
    }
    else if (strcmp(parameter, "yes") == 0)
    {
        o.m->remove_unreferenced_page_resources = QPDFJob::re_yes;
    }
    else if (strcmp(parameter, "no") == 0)
    {
        o.m->remove_unreferenced_page_resources = QPDFJob::re_no;
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
    QPDFJob::parse_object_id(
        parameter, o.m->show_trailer, o.m->show_obj, o.m->show_gen);
    o.m->require_outfile = false;
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

QPDFJob::Config&
QPDFJob::Config::rotate(char const* parameter)
{
    o.parseRotationParameter(parameter);
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
    this->config.o.m->attachments_to_copy.push_back(this->caf);
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

    this->config.o.m->attachments_to_add.push_back(this->att);
    return this->config;
}

QPDFJob::PagesConfig::PagesConfig(Config& c) :
    config(c)
{
}

std::shared_ptr<QPDFJob::PagesConfig>
QPDFJob::Config::pages()
{
    if (! o.m->page_specs.empty())
    {
        usage("--pages may only be specified one time");
    }
    return std::shared_ptr<PagesConfig>(new PagesConfig(*this));
}

QPDFJob::Config&
QPDFJob::PagesConfig::end()
{
    if (this->config.o.m->page_specs.empty())
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
    this->config.o.m->page_specs.push_back(
        QPDFJob::PageSpec(filename, password, range));
    return *this;
}

std::shared_ptr<QPDFJob::UOConfig>
QPDFJob::Config::overlay()
{
    o.m->under_overlay = &o.m->overlay;
    return std::shared_ptr<UOConfig>(new UOConfig(*this));
}

std::shared_ptr<QPDFJob::UOConfig>
QPDFJob::Config::underlay()
{
    o.m->under_overlay = &o.m->underlay;
    return std::shared_ptr<UOConfig>(new UOConfig(*this));
}

QPDFJob::UOConfig::UOConfig(Config& c) :
    config(c)
{
}

QPDFJob::Config&
QPDFJob::UOConfig::end()
{
    if (config.o.m->under_overlay->filename.empty())
    {
        usage(config.o.m->under_overlay->which + " file not specified");
    }
    config.o.m->under_overlay = 0;
    return this->config;
}

QPDFJob::UOConfig&
QPDFJob::UOConfig::path(char const* parameter)
{
    if (! config.o.m->under_overlay->filename.empty())
    {
        usage(config.o.m->under_overlay->which + " file already specified");
    }
    else
    {
        config.o.m->under_overlay->filename = parameter;
    }
    return *this;
}

QPDFJob::UOConfig&
QPDFJob::UOConfig::to(char const* parameter)
{
    config.o.parseNumrange(parameter, 0);
    config.o.m->under_overlay->to_nr = parameter;
    return *this;
}

QPDFJob::UOConfig&
QPDFJob::UOConfig::from(char const* parameter)
{
    if (strlen(parameter))
    {
        config.o.parseNumrange(parameter, 0);
    }
    config.o.m->under_overlay->from_nr = parameter;
    return *this;
}

QPDFJob::UOConfig&
QPDFJob::UOConfig::repeat(char const* parameter)
{
    if (strlen(parameter))
    {
        config.o.parseNumrange(parameter, 0);
    }
    config.o.m->under_overlay->repeat_nr = parameter;
    return *this;
}

QPDFJob::UOConfig&
QPDFJob::UOConfig::password(char const* parameter)
{
    config.o.m->under_overlay->password = QUtil::make_shared_cstr(parameter);
    return *this;
}

std::shared_ptr<QPDFJob::EncConfig>
QPDFJob::Config::encrypt(int keylen,
                         std::string const& user_password,
                         std::string const& owner_password)
{
    o.m->keylen = keylen;
    if (keylen == 256)
    {
        o.m->use_aes = true;
    }
    o.m->user_password = user_password;
    o.m->owner_password = owner_password;
    return std::shared_ptr<EncConfig>(new EncConfig(*this));
}

QPDFJob::EncConfig::EncConfig(Config& c) :
    config(c)
{
}

QPDFJob::Config&
QPDFJob::EncConfig::end()
{
    config.o.m->encrypt = true;
    config.o.m->decrypt = false;
    config.o.m->copy_encryption = false;
    return this->config;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::allowInsecure()
{
    config.o.m->allow_insecure = true;
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::accessibility(char const* parameter)
{
    config.o.m->r3_accessibility = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::extract(char const* parameter)
{
    if (config.o.m->keylen == 40)
    {
        config.o.m->r2_extract = (strcmp(parameter, "y") == 0);
    }
    else
    {
        config.o.m->r3_extract = (strcmp(parameter, "y") == 0);
    }
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::print(char const* parameter)
{
    if (config.o.m->keylen == 40)
    {
        config.o.m->r2_print = (strcmp(parameter, "y") == 0);
    }
    else if (strcmp(parameter, "full") == 0)
    {
        config.o.m->r3_print = qpdf_r3p_full;
    }
    else if (strcmp(parameter, "low") == 0)
    {
        config.o.m->r3_print = qpdf_r3p_low;
    }
    else if (strcmp(parameter, "none") == 0)
    {
        config.o.m->r3_print = qpdf_r3p_none;
    }
    else
    {
        usage("invalid print option");
    }
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::modify(char const* parameter)
{
    if (config.o.m->keylen == 40)
    {
        config.o.m->r2_modify = (strcmp(parameter, "y") == 0);
    }
    else if (strcmp(parameter, "all") == 0)
    {
        config.o.m->r3_assemble = true;
        config.o.m->r3_annotate_and_form = true;
        config.o.m->r3_form_filling = true;
        config.o.m->r3_modify_other = true;
    }
    else if (strcmp(parameter, "annotate") == 0)
    {
        config.o.m->r3_assemble = true;
        config.o.m->r3_annotate_and_form = true;
        config.o.m->r3_form_filling = true;
        config.o.m->r3_modify_other = false;
    }
    else if (strcmp(parameter, "form") == 0)
    {
        config.o.m->r3_assemble = true;
        config.o.m->r3_annotate_and_form = false;
        config.o.m->r3_form_filling = true;
        config.o.m->r3_modify_other = false;
    }
    else if (strcmp(parameter, "assembly") == 0)
    {
        config.o.m->r3_assemble = true;
        config.o.m->r3_annotate_and_form = false;
        config.o.m->r3_form_filling = false;
        config.o.m->r3_modify_other = false;
    }
    else if (strcmp(parameter, "none") == 0)
    {
        config.o.m->r3_assemble = false;
        config.o.m->r3_annotate_and_form = false;
        config.o.m->r3_form_filling = false;
        config.o.m->r3_modify_other = false;
    }
    else
    {
        usage("invalid modify option");
    }
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::cleartextMetadata()
{
    config.o.m->cleartext_metadata = true;
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::assemble(char const* parameter)
{
    config.o.m->r3_assemble = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::annotate(char const* parameter)
{
    if (config.o.m->keylen == 40)
    {
        config.o.m->r2_annotate = (strcmp(parameter, "y") == 0);
    }
    else
    {
        config.o.m->r3_annotate_and_form = (strcmp(parameter, "y") == 0);
    }
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::form(char const* parameter)
{
    config.o.m->r3_form_filling = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::modifyOther(char const* parameter)
{
    config.o.m->r3_modify_other = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::useAes(char const* parameter)
{
    config.o.m->use_aes = (strcmp(parameter, "y") == 0);
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::forceV4()
{
    config.o.m->force_V4 = true;
    return *this;
}

QPDFJob::EncConfig&
QPDFJob::EncConfig::forceR5()
{
    config.o.m->force_R5 = true;
    return *this;
}

#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>
#include <cstring>

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
QPDFJob::CopyAttConfig::filename(char const* parameter)
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
        throw std::runtime_error("copy attachments: no path specified");
    }
    this->config.o.attachments_to_copy.push_back(this->caf);
    return this->config;
}

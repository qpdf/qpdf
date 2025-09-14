#ifndef QPDFJOB_PRIVATE_HH
#define QPDFJOB_PRIVATE_HH

#include <qpdf/QPDFJob.hh>

#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/QPDFLogger.hh>

// A selection of pages from a single input PDF to be included in the output. This corresponds to a
// single clause in the --pages option.
struct QPDFJob::Selection
{
    Selection(std::string const& filename, std::string const& password, std::string const& range) :
        filename(filename),
        password(password),
        range(range)
    {
    }

    Selection(QPDFJob::Selection const& other, int page) :
        filename(other.filename),
        // range and password are no longer required when this constructor is called.
        qpdf(other.qpdf),
        orig_pages(other.orig_pages),
        selected_pages({page})
    {
    }

    std::string filename;
    std::string password;
    std::string range;
    QPDF* qpdf;
    std::vector<QPDFObjectHandle> orig_pages;
    std::vector<int> selected_pages;
};

struct QPDFJob::RotationSpec
{
    RotationSpec(int angle = 0, bool relative = false) :
        angle(angle),
        relative(relative)
    {
    }

    int angle;
    bool relative;
};

struct QPDFJob::UnderOverlay
{
    UnderOverlay(char const* which) :
        which(which),
        to_nr("1-z"),
        from_nr("1-z"),
        repeat_nr("")
    {
    }

    std::string which;
    std::string filename;
    std::string password;
    std::string to_nr;
    std::string from_nr;
    std::string repeat_nr;
    std::unique_ptr<QPDF> pdf;
    std::vector<int> to_pagenos;
    std::vector<int> from_pagenos;
    std::vector<int> repeat_pagenos;
};

struct QPDFJob::PageLabelSpec
{
    PageLabelSpec(
        int first_page, qpdf_page_label_e label_type, int start_num, std::string_view prefix) :
        first_page(first_page),
        label_type(label_type),
        start_num(start_num),
        prefix(prefix)
    {
    }
    int first_page;
    qpdf_page_label_e label_type;
    int start_num{1};
    std::string prefix;
};

class QPDFJob::Members
{
    friend class QPDFJob;

  public:
    Members() :
        log(QPDFLogger::defaultLogger())
    {
    }
    Members(Members const&) = delete;
    ~Members() = default;

  private:
    // These default values are duplicated in help and docs.
    static int constexpr DEFAULT_KEEP_FILES_OPEN_THRESHOLD = 200;
    static int constexpr DEFAULT_OI_MIN_WIDTH = 128;
    static int constexpr DEFAULT_OI_MIN_HEIGHT = 128;
    static int constexpr DEFAULT_OI_MIN_AREA = 16384;
    static int constexpr DEFAULT_II_MIN_BYTES = 1024;

    std::shared_ptr<QPDFLogger> log;
    std::string message_prefix{"qpdf"};
    bool warnings{false};
    unsigned long encryption_status{0};
    bool verbose{false};
    std::string password;
    bool linearize{false};
    bool decrypt{false};
    bool remove_restrictions{false};
    int split_pages{0};
    bool progress{false};
    std::function<void(int)> progress_handler{nullptr};
    bool suppress_warnings{false};
    bool warnings_exit_zero{false};
    bool copy_encryption{false};
    std::string encryption_file;
    std::string encryption_file_password;
    bool encrypt{false};
    bool password_is_hex_key{false};
    bool suppress_password_recovery{false};
    password_mode_e password_mode{pm_auto};
    bool allow_insecure{false};
    bool allow_weak_crypto{false};
    std::string user_password;
    std::string owner_password;
    int keylen{0};
    bool r2_print{true};
    bool r2_modify{true};
    bool r2_extract{true};
    bool r2_annotate{true};
    bool r3_accessibility{true};
    bool r3_extract{true};
    bool r3_assemble{true};
    bool r3_annotate_and_form{true};
    bool r3_form_filling{true};
    bool r3_modify_other{true};
    qpdf_r3_print_e r3_print{qpdf_r3p_full};
    bool force_V4{false};
    bool force_R5{false};
    bool cleartext_metadata{false};
    bool use_aes{false};
    bool stream_data_set{false};
    qpdf_stream_data_e stream_data_mode{qpdf_s_compress};
    bool compress_streams{true};
    bool compress_streams_set{false};
    bool recompress_flate{false};
    bool recompress_flate_set{false};
    int compression_level{-1};
    int jpeg_quality{-1};
    qpdf_stream_decode_level_e decode_level{qpdf_dl_generalized};
    bool decode_level_set{false};
    bool normalize_set{false};
    bool normalize{false};
    bool suppress_recovery{false};
    bool object_stream_set{false};
    qpdf_object_stream_e object_stream_mode{qpdf_o_preserve};
    bool ignore_xref_streams{false};
    bool qdf_mode{false};
    bool preserve_unreferenced_objects{false};
    remove_unref_e remove_unreferenced_page_resources{re_auto};
    bool keep_files_open{true};
    bool keep_files_open_set{false};
    size_t keep_files_open_threshold{DEFAULT_KEEP_FILES_OPEN_THRESHOLD};
    bool newline_before_endstream{false};
    std::string linearize_pass1;
    bool coalesce_contents{false};
    bool flatten_annotations{false};
    int flatten_annotations_required{0};
    int flatten_annotations_forbidden{an_invisible | an_hidden};
    bool generate_appearances{false};
    PDFVersion max_input_version;
    std::string min_version;
    std::string force_version;
    bool show_npages{false};
    bool deterministic_id{false};
    bool static_id{false};
    bool static_aes_iv{false};
    bool suppress_original_object_id{false};
    bool show_encryption{false};
    bool show_encryption_key{false};
    bool check_linearization{false};
    bool show_linearization{false};
    bool show_xref{false};
    bool show_trailer{false};
    int show_obj{0};
    int show_gen{0};
    bool show_raw_stream_data{false};
    bool show_filtered_stream_data{false};
    bool show_pages{false};
    bool show_page_images{false};
    std::vector<size_t> collate;
    bool flatten_rotation{false};
    bool list_attachments{false};
    std::string attachment_to_show;
    std::list<std::string> attachments_to_remove;
    std::list<AddAttachment> attachments_to_add;
    std::list<CopyAttachmentFrom> attachments_to_copy;
    int json_version{0};
    std::set<std::string> json_keys;
    std::set<std::string> json_objects;
    qpdf_json_stream_data_e json_stream_data{qpdf_sj_none};
    bool json_stream_data_set{false};
    std::string json_stream_prefix;
    bool test_json_schema{false};
    bool check{false};
    bool optimize_images{false};
    bool externalize_inline_images{false};
    bool keep_inline_images{false};
    bool remove_info{false};
    bool remove_metadata{false};
    bool remove_page_labels{false};
    bool remove_structure{false};
    size_t oi_min_width{DEFAULT_OI_MIN_WIDTH};
    size_t oi_min_height{DEFAULT_OI_MIN_HEIGHT};
    size_t oi_min_area{DEFAULT_OI_MIN_AREA};
    size_t ii_min_bytes{DEFAULT_II_MIN_BYTES};
    std::vector<UnderOverlay> underlay;
    std::vector<UnderOverlay> overlay;
    UnderOverlay* under_overlay{nullptr};
    std::vector<Selection> selections;
    std::map<std::string, RotationSpec> rotations;
    bool require_outfile{true};
    bool replace_input{false};
    bool check_is_encrypted{false};
    bool check_requires_password{false};
    std::string infilename;
    bool empty_input{false};
    std::string outfilename;
    bool json_input{false};
    bool json_output{false};
    std::string update_from_json;
    bool report_mem_usage{false};
    std::vector<PageLabelSpec> page_label_specs;
};

#endif // QPDFOBJECT_PRIVATE_HH

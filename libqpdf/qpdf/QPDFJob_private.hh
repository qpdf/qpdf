#ifndef QPDFJOB_PRIVATE_HH
#define QPDFJOB_PRIVATE_HH

#include <qpdf/QPDFJob.hh>

#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDF_private.hh>

// A selection of pages from a single input PDF to be included in the output. This corresponds to a
// single clause in the --pages option.
struct QPDFJob::Selection
{
    Selection() = delete;

    Selection(std::pair<const std::string, Input>& entry);

    Selection(Selection const& other, int page) :
        in_entry(other.in_entry),
        selected_pages({page})
    {
    }

    QPDFJob::Input& input();
    std::string const& filename();
    void password(std::string password);

    std::pair<const std::string, QPDFJob::Input>* in_entry{nullptr};
    std::string range; // An empty range means all pages.
    std::vector<int> selected_pages;
};

// A single input PDF.
//
// N.B. A single input PDF may be represented by multiple Input instances using variations of the
// filename.  This is a documented work-around.
struct QPDFJob::Input
{
    void initialize(Inputs& in, QPDF* qpdf = nullptr);

    std::string password;
    std::unique_ptr<QPDF> qpdf_p;
    QPDF* qpdf;
    ClosedFileInputSource* cfis{};
    std::vector<QPDFObjectHandle> orig_pages;
    int n_pages;
    std::vector<bool> copied_pages;
    bool remove_unreferenced{false};
};

// All PDF input files for a job.
struct QPDFJob::Inputs
{
    friend struct Input;
    // These default values are duplicated in help and docs.
    static int constexpr DEFAULT_KEEP_FILES_OPEN_THRESHOLD = 200;

    Inputs(QPDFJob& job) :
        job(job)
    {
    }
    void process(std::string const& filename, QPDFJob::Input& file_spec);
    void process_all();

    // Destroy all owned QPDF objects. Return false if any of the QPDF objects recorded warnings.
    bool clear();

    Selection& new_selection(std::string const& filename);

    void new_selection(
        std::string const& filename, std::string const& password, std::string const& range);

    std::string const&
    infile_name() const
    {
        return infile_name_;
    }

    void infile_name(std::string const& name);

    std::string infile_name_;
    std::string encryption_file;
    std::string encryption_file_password;
    bool keep_files_open{true};
    bool keep_files_open_set{false};
    size_t keep_files_open_threshold{DEFAULT_KEEP_FILES_OPEN_THRESHOLD};

    std::map<std::string, Input> files;
    std::vector<Selection> selections;

    bool any_page_labels{false};

  private:
    QPDFJob& job;
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
    Members(QPDFJob& job) :
        log(qcf.log()),
        inputs(job)
    {
    }
    Members(Members const&) = delete;
    ~Members() = default;

    inline const char* infile_nm() const;

    inline std::string const& infile_name() const;

  private:
    // These default values are duplicated in help and docs.
    static int constexpr DEFAULT_OI_MIN_WIDTH = 128;
    static int constexpr DEFAULT_OI_MIN_HEIGHT = 128;
    static int constexpr DEFAULT_OI_MIN_AREA = 16384;
    static int constexpr DEFAULT_II_MIN_BYTES = 1024;

    qpdf::Doc::Config qcf;
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
    bool warnings_exit_zero{false};
    bool copy_encryption{false};
    bool encrypt{false};
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
    bool object_stream_set{false};
    qpdf_object_stream_e object_stream_mode{qpdf_o_preserve};
    bool qdf_mode{false};
    bool preserve_unreferenced_objects{false};
    remove_unref_e remove_unreferenced_page_resources{re_auto};
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
    Inputs inputs;
    std::map<std::string, RotationSpec> rotations;
    bool require_outfile{true};
    bool replace_input{false};
    bool check_is_encrypted{false};
    bool check_requires_password{false};
    bool empty_input{false};
    std::string outfilename;
    bool json_input{false};
    bool json_output{false};
    std::string update_from_json;
    bool report_mem_usage{false};
    std::vector<PageLabelSpec> page_label_specs;
};

inline const char*
QPDFJob::Members::infile_nm() const
{
    return inputs.infile_name().data();
}

inline std::string const&
QPDFJob::Members::infile_name() const
{
    return inputs.infile_name();
}

#endif // QPDFJOB_PRIVATE_HH

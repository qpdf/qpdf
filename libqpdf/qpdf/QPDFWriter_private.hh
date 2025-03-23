#ifndef QPDFWRITER_PRIVATE_HH
#define QPDFWRITER_PRIVATE_HH

#include <qpdf/QPDFWriter.hh>

#include <qpdf/ObjTable.hh>
#include <qpdf/Pipeline_private.hh>

// This file is intended for inclusion by QPDFWriter, QPDF, QPDF_optimization and QPDF_linearization
// only.

struct QPDFWriter::Object
{
    int renumber{0};
    int gen{0};
    int object_stream{0};
};

struct QPDFWriter::NewObject
{
    QPDFXRefEntry xref;
    qpdf_offset_t length{0};
};

class QPDFWriter::ObjTable: public ::ObjTable<QPDFWriter::Object>
{
    friend class QPDFWriter;

  public:
    bool
    getStreamsEmpty() const noexcept
    {
        return streams_empty;
    }

  private:
    // For performance, set by QPDFWriter rather than tracked by ObjTable.
    bool streams_empty{false};
};

class QPDFWriter::NewObjTable: public ::ObjTable<QPDFWriter::NewObject>
{
    friend class QPDFWriter;
};

class QPDFWriter::Members
{
    friend class QPDFWriter;

  public:
    QPDF_DLL
    ~Members();

  private:
    Members(QPDF& pdf);
    Members(Members const&) = delete;

    QPDF& pdf;
    QPDFObjGen root_og{-1, 0};
    char const* filename{"unspecified"};
    FILE* file{nullptr};
    bool close_file{false};
    Pl_Buffer* buffer_pipeline{nullptr};
    Buffer* output_buffer{nullptr};
    bool normalize_content_set{false};
    bool normalize_content{false};
    bool compress_streams{true};
    bool compress_streams_set{false};
    qpdf_stream_decode_level_e stream_decode_level{qpdf_dl_generalized};
    bool stream_decode_level_set{false};
    bool recompress_flate{false};
    bool qdf_mode{false};
    bool preserve_unreferenced_objects{false};
    bool newline_before_endstream{false};
    bool static_id{false};
    bool suppress_original_object_ids{false};
    bool direct_stream_lengths{true};
    bool encrypted{false};
    bool preserve_encryption{true};
    bool linearized{false};
    bool pclm{false};
    qpdf_object_stream_e object_stream_mode{qpdf_o_preserve};
    std::string encryption_key;
    bool encrypt_metadata{true};
    bool encrypt_use_aes{false};
    std::map<std::string, std::string> encryption_dictionary;
    int encryption_V{0};
    int encryption_R{0};

    std::string id1; // for /ID key of
    std::string id2; // trailer dictionary
    std::string final_pdf_version;
    int final_extension_level{0};
    std::string min_pdf_version;
    int min_extension_level{0};
    std::string forced_pdf_version;
    int forced_extension_level{0};
    std::string extra_header_text;
    int encryption_dict_objid{0};
    std::string cur_data_key;
    std::list<std::shared_ptr<Pipeline>> to_delete;
    qpdf::pl::Count* pipeline{nullptr};
    std::vector<QPDFObjectHandle> object_queue;
    size_t object_queue_front{0};
    QPDFWriter::ObjTable obj;
    QPDFWriter::NewObjTable new_obj;
    int next_objid{1};
    int cur_stream_length_id{0};
    size_t cur_stream_length{0};
    bool added_newline{false};
    size_t max_ostream_index{0};
    std::set<QPDFObjGen> normalized_streams;
    std::map<QPDFObjGen, int> page_object_to_seq;
    std::map<QPDFObjGen, int> contents_to_page_seq;
    std::map<int, std::vector<QPDFObjGen>> object_stream_to_objects;
    std::vector<Pipeline*> pipeline_stack;
    unsigned long long next_stack_id{0};
    bool deterministic_id{false};
    Pl_MD5* md5_pipeline{nullptr};
    std::string deterministic_id_data;
    bool did_write_setup{false};

    // For linearization only
    std::string lin_pass1_filename;

    // For progress reporting
    std::shared_ptr<QPDFWriter::ProgressReporter> progress_reporter;
    int events_expected{0};
    int events_seen{0};
    int next_progress_report{0};
};

#endif // QPDFWRITER_PRIVATE_HH

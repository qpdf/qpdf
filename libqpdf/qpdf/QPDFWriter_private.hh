#ifndef QPDFWRITER_PRIVATE_HH
#define QPDFWRITER_PRIVATE_HH

#include <qpdf/QPDFWriter.hh>

#include <qpdf/ObjTable.hh>
#include <qpdf/Pipeline_private.hh>
#include <qpdf/QPDF.hh>

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

namespace qpdf
{
    namespace impl
    {
        class Writer;
    }

    class Writer
    {
      public:
        class Config
        {
            friend class impl::Writer;
            friend class ::QPDFWriter;

            std::string forced_pdf_version_;
            std::string extra_header_text_;
            // For linearization only
            std::string lin_pass1_filename_;

            qpdf_object_stream_e object_stream_mode_{qpdf_o_preserve};

            int forced_extension_level_{0};

            bool normalize_content_set_{false};
            bool normalize_content_{false};
            bool compress_streams_{true};
            bool compress_streams_set_{false};
            qpdf_stream_decode_level_e stream_decode_level_{qpdf_dl_generalized};
            bool stream_decode_level_set_{false};
            bool recompress_flate_{false};
            bool qdf_mode_{false};
            bool preserve_unreferenced_objects_{false};
            bool newline_before_endstream_{false};
            bool deterministic_id_{false};
            bool static_id_{false};
            bool suppress_original_object_ids_{false};
            bool direct_stream_lengths_{true};
            bool preserve_encryption_{true};
            bool linearized_{false};
            bool pclm_{false};
            bool encrypt_use_aes_{false};
        }; // class Writer::Config
    }; // class Writer
} // namespace qpdf

#endif // QPDFWRITER_PRIVATE_HH

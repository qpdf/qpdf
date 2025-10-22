#ifndef QPDFWRITER_PRIVATE_HH
#define QPDFWRITER_PRIVATE_HH

#include <qpdf/QPDFWriter.hh>

#include <qpdf/ObjTable.hh>
#include <qpdf/Pipeline_private.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFUsage.hh>

// This file is intended for inclusion by QPDFWriter, QPDF, QPDF_optimization and QPDF_linearization
// only.

namespace qpdf
{
    namespace impl
    {
        class Writer;
    }

    class Writer: public QPDFWriter
    {
      public:
        class Config
        {
          public:
            Config() = default;
            Config(Config const&) = default;
            Config(Config&&) = delete;
            Config& operator=(Config const&) = default;
            Config& operator=(Config&&) = delete;
            ~Config() = default;

            Config(bool permissive) :
                permissive_(permissive)
            {
            }

            bool
            linearize() const
            {
                return linearize_;
            }

            Config& linearize(bool val);

            std::string const&
            linearize_pass1() const
            {
                return linearize_pass1_;
            }

            Config&
            linearize_pass1(std::string const& val)
            {
                linearize_pass1_ = val;
                return *this;
            }

            bool
            preserve_encryption() const
            {
                return preserve_encryption_;
            }

            Config&
            preserve_encryption(bool val)
            {
                preserve_encryption_ = val;
                return *this;
            }

            bool
            encrypt_use_aes() const
            {
                return encrypt_use_aes_;
            }

            Config&
            encrypt_use_aes(bool val)
            {
                encrypt_use_aes_ = val;
                return *this;
            }

            Config&
            default_decode_level(qpdf_stream_decode_level_e val)
            {
                if (!decode_level_set_) {
                    decode_level_ = val;
                }
                return *this;
            }

            qpdf_stream_decode_level_e
            decode_level() const
            {
                return decode_level_;
            }

            Config& decode_level(qpdf_stream_decode_level_e val);

            qpdf_object_stream_e
            object_streams() const
            {
                return object_streams_;
            }

            Config&
            object_streams(qpdf_object_stream_e val)
            {
                object_streams_ = val;
                return *this;
            }

            bool
            compress_streams() const
            {
                return compress_streams_;
            }

            Config& compress_streams(bool val);

            bool
            direct_stream_lengths() const
            {
                return direct_stream_lengths_;
            }

            bool
            newline_before_endstream() const
            {
                return newline_before_endstream_;
            }

            Config&
            newline_before_endstream(bool val)
            {
                newline_before_endstream_ = val;
                return *this;
            }

            bool
            recompress_flate() const
            {
                return recompress_flate_;
            }

            Config&
            recompress_flate(bool val)
            {
                recompress_flate_ = val;
                return *this;
            }

            Config& stream_data(qpdf_stream_data_e val);

            std::string const&
            forced_pdf_version() const
            {
                return forced_pdf_version_;
            }

            Config&
            forced_pdf_version(std::string const& val)
            {
                forced_pdf_version_ = val;
                return *this;
            }

            Config&
            forced_pdf_version(std::string const& val, int ext)
            {
                forced_pdf_version_ = val;
                forced_extension_level_ = ext;
                return *this;
            }

            int
            forced_extension_level() const
            {
                return forced_extension_level_;
            }

            Config&
            forced_extension_level(int val)
            {
                forced_extension_level_ = val;
                return *this;
            }

            std::string const&
            extra_header_text()
            {
                return extra_header_text_;
            }

            Config& extra_header_text(std::string const& val);

            bool
            preserve_unreferenced() const
            {
                return preserve_unreferenced_;
            }

            Config&
            preserve_unreferenced(bool val)
            {
                preserve_unreferenced_ = val;
                return *this;
            }

            bool
            no_original_object_ids() const
            {
                return no_original_object_ids_;
            }

            Config&
            no_original_object_ids(bool val)
            {
                no_original_object_ids_ = val;
                return *this;
            }

            bool
            qdf() const
            {
                return qdf_;
            }

            Config& qdf(bool val);

            bool
            normalize_content() const
            {
                return normalize_content_;
            }

            Config&
            normalize_content(bool val)
            {
                normalize_content_ = val;
                normalize_content_set_ = true;
                return *this;
            }

            bool
            deterministic_id() const
            {
                return deterministic_id_;
            }

            Config&
            deterministic_id(bool val)
            {
                deterministic_id_ = val;
                return *this;
            }

            bool
            static_id() const
            {
                return static_id_;
            }

            Config&
            static_id(bool val)
            {
                static_id_ = val;
                return *this;
            }

            bool
            pclm() const
            {
                return pclm_;
            }

            Config& pclm(bool val);

          private:
            void
            usage(std::string const& msg) const
            {
                if (!permissive_) {
                    throw QPDFUsage(msg);
                }
            }

            std::string forced_pdf_version_;
            std::string extra_header_text_;
            // For linearization only
            std::string linearize_pass1_;

            qpdf_object_stream_e object_streams_{qpdf_o_preserve};
            qpdf_stream_decode_level_e decode_level_{qpdf_dl_generalized};

            int forced_extension_level_{0};

            bool normalize_content_set_{false};
            bool normalize_content_{false};
            bool compress_streams_{true};
            bool compress_streams_set_{false};
            bool decode_level_set_{false};
            bool recompress_flate_{false};
            bool qdf_{false};
            bool preserve_unreferenced_{false};
            bool newline_before_endstream_{false};
            bool deterministic_id_{false};
            bool static_id_{false};
            bool no_original_object_ids_{false};
            bool direct_stream_lengths_{true};
            bool preserve_encryption_{true};
            bool linearize_{false};
            bool pclm_{false};
            bool encrypt_use_aes_{false};

            bool permissive_{true};
        }; // class Writer::Config

        Writer() = delete;
        Writer(Writer const&) = delete;
        Writer(Writer&&) = delete;
        Writer& operator=(Writer const&) = delete;
        Writer& operator=(Writer&&) = delete;
        ~Writer() = default;

        Writer(QPDF& qpdf, Config cfg);

    }; // class Writer
} // namespace qpdf

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
    friend class qpdf::impl::Writer;

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

#endif // QPDFWRITER_PRIVATE_HH

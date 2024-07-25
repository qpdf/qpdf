#include <qpdf/qpdf-c.h>

#include <memory>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFWriter.hh>

struct _qpdf_error
{
    std::shared_ptr<QPDFExc> exc;
};

struct _qpdf_data
{
    _qpdf_data() = default;

    _qpdf_data(std::unique_ptr<QPDF>&& qpdf) :
        qpdf(std::move(qpdf)) {};

    ~_qpdf_data() = default;

    std::shared_ptr<QPDF> qpdf;
    std::shared_ptr<QPDFWriter> qpdf_writer;

    std::shared_ptr<QPDFExc> error;
    _qpdf_error tmp_error;
    std::list<QPDFExc> warnings;
    std::string tmp_string;

    // Parameters for functions we call
    char const* filename{nullptr}; // or description
    char const* buffer{nullptr};
    unsigned long long size{0};
    char const* password{nullptr};
    bool write_memory{false};
    std::shared_ptr<Buffer> output_buffer;

    // QPDFObjectHandle support
    bool silence_errors{false};
    bool oh_error_occurred{false};
    std::map<qpdf_oh, std::shared_ptr<QPDFObjectHandle>> oh_cache;
    qpdf_oh next_oh{0};
    std::set<std::string> cur_iter_dict_keys;
    std::set<std::string>::const_iterator dict_iter;
    std::string cur_dict_key;
};

#include <qpdf/global_private.hh>

#include <qpdf/Util.hh>

using namespace qpdf;
using namespace qpdf::global;

Limits Limits::l;
Options Options::o;

void
Limits::parser_max_container_size(bool damaged, uint32_t value)
{
    if (damaged) {
        l.parser_max_container_size_damaged_set_ = true;
        l.parser_max_container_size_damaged_ = value;
    } else {
        l.parser_max_container_size_ = value;
    }
}

void
Limits::disable_defaults()
{
    if (!l.parser_max_errors_set_) {
        l.parser_max_errors_ = 0;
    }
    if (!l.parser_max_container_size_damaged_set_) {
        l.parser_max_container_size_damaged_ = std::numeric_limits<uint32_t>::max();
    }
}

qpdf_result_e
qpdf_global_get_uint32(qpdf_param_e param, uint32_t* value)
{
    qpdf_expect(value);
    switch (param) {
    case qpdf_p_default_limits:
        *value = Options::default_limits();
        return qpdf_r_ok;
    case qpdf_p_limit_errors:
        *value = Limits::errors();
        return qpdf_r_ok;
    case qpdf_p_parser_max_nesting:
        *value = Limits::parser_max_nesting();
        return qpdf_r_ok;
    case qpdf_p_parser_max_errors:
        *value = Limits::parser_max_errors();
        return qpdf_r_ok;
    case qpdf_p_parser_max_container_size:
        *value = Limits::parser_max_container_size(false);
        return qpdf_r_ok;
    case qpdf_p_parser_max_container_size_damaged:
        *value = Limits::parser_max_container_size(true);
        return qpdf_r_ok;
    default:
        return qpdf_r_bad_parameter;
    }
}

qpdf_result_e
qpdf_global_set_uint32(qpdf_param_e param, uint32_t value)
{
    switch (param) {
    case qpdf_p_default_limits:
        Options::default_limits(value);
        return qpdf_r_ok;
    case qpdf_p_parser_max_nesting:
        Limits::parser_max_nesting(value);
        return qpdf_r_ok;
    case qpdf_p_parser_max_errors:
        Limits::parser_max_errors(value);
        return qpdf_r_ok;
    case qpdf_p_parser_max_container_size:
        Limits::parser_max_container_size(false, value);
        return qpdf_r_ok;
    case qpdf_p_parser_max_container_size_damaged:
        Limits::parser_max_container_size(true, value);
        return qpdf_r_ok;
    default:
        return qpdf_r_bad_parameter;
    }
}

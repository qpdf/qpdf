#include <qpdf/global_private.hh>

#include <qpdf/Util.hh>

using namespace qpdf;
using namespace qpdf::global;

Limits Limits::l;
Options Options::o;

void
Limits::objects_max_container_size(bool damaged, uint32_t value)
{
    if (damaged) {
        l.objects_max_container_size_damaged_set_ = true;
        l.objects_max_container_size_damaged_ = value;
    } else {
        l.objects_max_container_size_ = value;
    }
}

void
Limits::disable_defaults()
{
    if (!l.objects_max_errors_set_) {
        l.objects_max_errors_ = 0;
    }
    if (!l.objects_max_container_size_damaged_set_) {
        l.objects_max_container_size_damaged_ = std::numeric_limits<uint32_t>::max();
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
    case qpdf_p_objects_max_nesting:
        *value = Limits::objects_max_nesting();
        return qpdf_r_ok;
    case qpdf_p_objects_max_errors:
        *value = Limits::objects_max_errors();
        return qpdf_r_ok;
    case qpdf_p_objects_max_container_size:
        *value = Limits::objects_max_container_size(false);
        return qpdf_r_ok;
    case qpdf_p_objects_max_container_size_damaged:
        *value = Limits::objects_max_container_size(true);
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
    case qpdf_p_objects_max_nesting:
        Limits::objects_max_nesting(value);
        return qpdf_r_ok;
    case qpdf_p_objects_max_errors:
        Limits::objects_max_errors(value);
        return qpdf_r_ok;
    case qpdf_p_objects_max_container_size:
        Limits::objects_max_container_size(false, value);
        return qpdf_r_ok;
    case qpdf_p_objects_max_container_size_damaged:
        Limits::objects_max_container_size(true, value);
        return qpdf_r_ok;
    default:
        return qpdf_r_bad_parameter;
    }
}

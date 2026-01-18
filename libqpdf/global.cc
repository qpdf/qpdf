#include <qpdf/global_private.hh>

#include <qpdf/Util.hh>

using namespace qpdf;
using namespace qpdf::global;

Limits Limits::l;
Options Options::o;

void
Options::fuzz_mode(bool value)
{
    if (value) {
        o.fuzz_mode_ = true;
        // Limit the memory used to decompress JPEG files during fuzzing. Excessive memory use
        // during fuzzing is due to corrupt JPEG data which sometimes cannot be detected before
        // jpeg_start_decompress is called. During normal use of qpdf very large JPEGs can
        // occasionally occur legitimately and therefore must be allowed during normal operations.
        Limits::dct_max_memory(10'000'000);
        Limits::dct_max_progressive_scans(50);
        // Do not decompress corrupt data. This may cause extended runtime within jpeglib without
        // exercising additional code paths in qpdf.
        dct_throw_on_corrupt_data(true);
        Limits::png_max_memory(1'000'000);
        Limits::flate_max_memory(200'000);
        Limits::run_length_max_memory(1'000'000);
    }
}

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
    if (!l.max_stream_filters_set_) {
        l.max_stream_filters_ = std::numeric_limits<uint32_t>::max();
    }
}

qpdf_result_e
qpdf_global_get_uint32(qpdf_param_e param, uint32_t* value)
{
    qpdf_expect(value);
    switch (param) {
    case qpdf_p_inspection_mode:
        *value = Options::inspection_mode();
        return qpdf_r_ok;
    case qpdf_p_fuzz_mode:
        *value = Options::fuzz_mode();
        return qpdf_r_ok;
    case qpdf_p_default_limits:
        *value = Options::default_limits();
        return qpdf_r_ok;
    case qpdf_p_limit_errors:
        *value = Limits::errors();
        return qpdf_r_ok;
    case qpdf_p_dct_throw_on_corrupt_data:
        *value = Options::dct_throw_on_corrupt_data();
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
    case qpdf_p_max_stream_filters:
        *value = Limits::max_stream_filters();
        return qpdf_r_ok;
    case qpdf_p_dct_max_memory:
        qpdf_invariant(util::fits<uint32_t>(Limits::dct_max_memory()));
        *value = static_cast<uint32_t>(Limits::dct_max_memory());
        return qpdf_r_ok;
    case qpdf_p_dct_max_progressive_scans:
        qpdf_invariant(util::fits<uint32_t>(Limits::dct_max_progressive_scans()));
        *value = static_cast<uint32_t>(Limits::dct_max_progressive_scans());
        return qpdf_r_ok;
    case qpdf_p_flate_max_memory:
        qpdf_invariant(util::fits<uint32_t>(Limits::flate_max_memory()));
        *value = static_cast<uint32_t>(Limits::flate_max_memory());
        return qpdf_r_ok;
    case qpdf_p_png_max_memory:
        qpdf_invariant(util::fits<uint32_t>(Limits::png_max_memory()));
        *value = static_cast<uint32_t>(Limits::png_max_memory());
        return qpdf_r_ok;
    case qpdf_p_run_length_max_memory:
        qpdf_invariant(util::fits<uint32_t>(Limits::run_length_max_memory()));
        *value = static_cast<uint32_t>(Limits::run_length_max_memory());
        return qpdf_r_ok;
    default:
        return qpdf_r_bad_parameter;
    }
}

qpdf_result_e
qpdf_global_set_uint32(qpdf_param_e param, uint32_t value)
{
    switch (param) {
    case qpdf_p_inspection_mode:
        Options::inspection_mode(value);
        return qpdf_r_ok;
    case qpdf_p_fuzz_mode:
        Options::fuzz_mode(value);
        return qpdf_r_ok;
    case qpdf_p_default_limits:
        Options::default_limits(value);
        return qpdf_r_ok;
    case qpdf_p_dct_throw_on_corrupt_data:
        Options::dct_throw_on_corrupt_data(value);
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
    case qpdf_p_max_stream_filters:
        Limits::max_stream_filters(value);
        return qpdf_r_ok;
    case qpdf_p_dct_max_memory:
        Limits::dct_max_memory(util::fits<long>(value) ? static_cast<long>(value) : 0);
        return qpdf_r_ok;
    case qpdf_p_dct_max_progressive_scans:
        Limits::dct_max_progressive_scans(util::fits<int>(value) ? static_cast<int>(value) : 0);
        return qpdf_r_ok;
    case qpdf_p_flate_max_memory:
        Limits::flate_max_memory(value);
        return qpdf_r_ok;
    case qpdf_p_png_max_memory:
        Limits::png_max_memory(value);
        return qpdf_r_ok;
    case qpdf_p_run_length_max_memory:
        Limits::run_length_max_memory(value);
        return qpdf_r_ok;
    default:
        return qpdf_r_bad_parameter;
    }
}

#include <qpdf/Pl_TIFFPredictor.hh>

#include <qpdf/BitStream.hh>
#include <qpdf/BitWriter.hh>
#include <qpdf/QTC.hh>
#include <qpdf/Util.hh>
#include <qpdf/global_private.hh>

#include <climits>
#include <stdexcept>

using namespace qpdf;

namespace
{
    unsigned long long const& memory_limit = global::Limits::tiff_max_memory();
} // namespace

Pl_TIFFPredictor::Pl_TIFFPredictor(
    char const* identifier,
    Pipeline* next,
    action_e action,
    uint32_t columns,
    uint32_t samples_per_pixel,
    uint32_t bits_per_sample) :
    Pipeline(identifier, next),
    action(action),
    columns(columns),
    samples_per_pixel(samples_per_pixel),
    bits_per_sample(bits_per_sample)
{
    util::assertion(next, "Attempt to create Pl_TIFFPredictor with nullptr as next");
    util::no_ci_rt_error_if(
        samples_per_pixel < 1, "TIFFPredictor created with invalid samples_per_pixel");
    util::no_ci_rt_error_if(
        bits_per_sample < 1 || bits_per_sample > (8 * (sizeof(unsigned long long))),
        "TIFFPredictor created with invalid bits_per_sample");
    auto bits_per_pixel = 1ULL * bits_per_sample * samples_per_pixel;
    util::no_ci_rt_error_if(
        !util::fits<uint32_t>(bits_per_pixel + 7),
        "TIFFPredictor created with bits_per_sample and samples_per_pixel values that cause "
        "overflow");
    auto bpr = (columns * bits_per_pixel + 7) / 8;
    util::no_ci_rt_error_if(
        bpr == 0 || !util::fits<uint32_t>(bpr), "TIFFPredictor created with invalid columns value");
    util::no_ci_rt_error_if(
        memory_limit > 0 && bpr > (memory_limit / 2ULL), "TIFFPredictor memory limit exceeded");
    bytes_per_row = static_cast<uint32_t>(bpr);
}

void
Pl_TIFFPredictor::setMemoryLimit(unsigned long long limit)
{
    global::Limits::tiff_max_memory(limit);
}

void
Pl_TIFFPredictor::write(unsigned char const* data, size_t len)
{
    auto end = data + len;
    auto row_end = data + (bytes_per_row - cur_row.size());
    while (row_end <= end) {
        // finish off current row
        cur_row.insert(cur_row.end(), data, row_end);
        data = row_end;
        row_end += bytes_per_row;

        processRow();

        // Prepare for next row
        cur_row.clear();
    }

    cur_row.insert(cur_row.end(), data, end);
}

void
Pl_TIFFPredictor::processRow()
{
    QTC::TC("libtests", "Pl_TIFFPredictor processRow", (action == a_decode ? 0 : 1));
    previous.assign(samples_per_pixel, 0);
    if (bits_per_sample != 8) {
        BitWriter bw(next());
        BitStream in(cur_row.data(), cur_row.size());
        for (uint32_t col = 0; col < this->columns; ++col) {
            for (auto& prev: previous) {
                long long sample = in.getBitsSigned(this->bits_per_sample);
                long long new_sample = sample;
                if (action == a_encode) {
                    new_sample -= prev;
                    prev = sample;
                } else {
                    new_sample += prev;
                    prev = new_sample;
                }
                bw.writeBitsSigned(new_sample, this->bits_per_sample);
            }
        }
        bw.flush();
    } else {
        out.clear();
        auto next_it = cur_row.begin();
        auto cr_end = cur_row.end();
        auto pr_end = previous.end();

        while (next_it != cr_end) {
            for (auto prev = previous.begin(); prev != pr_end && next_it != cr_end;
                 ++prev, ++next_it) {
                long long sample = *next_it;
                long long new_sample = sample;
                if (action == a_encode) {
                    new_sample -= *prev;
                    *prev = sample;
                } else {
                    new_sample += *prev;
                    *prev = new_sample;
                }
                out.push_back(static_cast<unsigned char>(255U & new_sample));
            }
        }
        next()->write(out.data(), out.size());
    }
}

void
Pl_TIFFPredictor::finish()
{
    if (!cur_row.empty()) {
        // write partial row
        cur_row.insert(cur_row.end(), bytes_per_row - cur_row.size(), 0);
        processRow();
    }
    cur_row.clear();
    next()->finish();
}

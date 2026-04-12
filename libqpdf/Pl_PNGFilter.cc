#include <qpdf/Pl_PNGFilter.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>
#include <qpdf/global_private.hh>

#include <climits>
#include <cstring>

using namespace qpdf;

namespace
{
    unsigned long long const& memory_limit = global::Limits::png_max_memory();
} // namespace

static int
abs_diff(int a, int b)
{
    return a > b ? a - b : b - a;
}

Pl_PNGFilter::Pl_PNGFilter(
    char const* identifier,
    Pipeline* next,
    action_e action,
    uint32_t columns,
    uint32_t samples_per_pixel,
    uint32_t bits_per_sample) :
    Pipeline(identifier, next),
    action(action)
{
    util::assertion(next, "Attempt to create Pl_PNGFilter with nullptr as next");
    util::no_ci_rt_error_if(
        samples_per_pixel < 1, "PNGFilter created with invalid samples_per_pixel");
    util::no_ci_rt_error_if(
        !(bits_per_sample == 1 || bits_per_sample == 2 || bits_per_sample == 4 ||
          bits_per_sample == 8 || bits_per_sample == 16),
        "PNGFilter created with invalid bits_per_sample not 1, 2, 4, 8, or 16");
    auto bits_per_pixel = 1ULL * bits_per_sample * samples_per_pixel;
    util::no_ci_rt_error_if(
        !util::fits<uint32_t>(bits_per_pixel + 7),
        "PNGFilter created with bits_per_sample and samples_per_pixel values that cause overflow");
    bytes_per_pixel = static_cast<uint32_t>((bits_per_pixel + 7) / 8);
    auto bpr = ((columns * bits_per_pixel) + 7) / 8;
    util::no_ci_rt_error_if(
        bpr == 0 || !util::fits<uint32_t>(bpr), "PNGFilter created with invalid columns value");
    util::no_ci_rt_error_if(
        memory_limit > 0 && bpr > (memory_limit / 2U), "PNGFilter memory limit exceeded");
    bytes_per_row = static_cast<uint32_t>(bpr);
    buf1 = QUtil::make_shared_array<unsigned char>(bytes_per_row + 1);
    buf2 = QUtil::make_shared_array<unsigned char>(bytes_per_row + 1);
    memset(buf1.get(), 0, bytes_per_row + 1);
    memset(buf2.get(), 0, bytes_per_row + 1);
    cur_row = buf1.get();
    prev_row = buf2.get();

    // number of bytes per incoming row
    incoming = (action == a_encode ? bytes_per_row : bytes_per_row + 1);
}

void
Pl_PNGFilter::setMemoryLimit(unsigned long long limit)
{
    global::Limits::png_max_memory(limit);
}

void
Pl_PNGFilter::write(unsigned char const* data, size_t len)
{
    size_t left = incoming - pos;
    size_t offset = 0;
    while (len >= left) {
        // finish off current row
        memcpy(cur_row + pos, data + offset, left);
        offset += left;
        len -= left;

        processRow();

        // Swap rows
        unsigned char* t = prev_row;
        prev_row = cur_row;
        cur_row = t ? t : buf2.get();
        memset(cur_row, 0, bytes_per_row + 1);
        left = incoming;
        pos = 0;
    }
    if (len) {
        memcpy(cur_row + pos, data + offset, len);
    }
    pos += len;
}

void
Pl_PNGFilter::processRow()
{
    if (action == a_encode) {
        encodeRow();
    } else {
        decodeRow();
    }
}

void
Pl_PNGFilter::decodeRow()
{
    int filter = cur_row[0];
    if (prev_row) {
        switch (filter) {
        case 0:
            break;
        case 1:
            decodeSub();
            break;
        case 2:
            decodeUp();
            break;
        case 3:
            decodeAverage();
            break;
        case 4:
            decodePaeth();
            break;
        default:
            // ignore
            break;
        }
    }

    next()->write(cur_row + 1, bytes_per_row);
}

void
Pl_PNGFilter::decodeSub()
{
    unsigned char* buffer = cur_row + 1;
    unsigned int bpp = bytes_per_pixel;

    for (unsigned int i = 0; i < bytes_per_row; ++i) {
        unsigned char left = 0;

        if (i >= bpp) {
            left = buffer[i - bpp];
        }

        buffer[i] = static_cast<unsigned char>(buffer[i] + left);
    }
}

void
Pl_PNGFilter::decodeUp()
{
    unsigned char* buffer = cur_row + 1;
    unsigned char* above_buffer = prev_row + 1;

    for (unsigned int i = 0; i < bytes_per_row; ++i) {
        unsigned char up = above_buffer[i];
        buffer[i] = static_cast<unsigned char>(buffer[i] + up);
    }
}

void
Pl_PNGFilter::decodeAverage()
{
    unsigned char* buffer = cur_row + 1;
    unsigned char* above_buffer = prev_row + 1;
    auto bpp = bytes_per_pixel;

    for (unsigned int i = 0; i < bytes_per_row; ++i) {
        int left = 0;
        int up = 0;

        if (i >= bpp) {
            left = buffer[i - bpp];
        }

        up = above_buffer[i];
        buffer[i] = static_cast<unsigned char>(buffer[i] + (left + up) / 2);
    }
}

void
Pl_PNGFilter::decodePaeth()
{
    unsigned char* buffer = cur_row + 1;
    unsigned char* above_buffer = prev_row + 1;
    auto bpp = bytes_per_pixel;

    for (unsigned int i = 0; i < bytes_per_row; ++i) {
        int left = 0;
        int up = above_buffer[i];
        int upper_left = 0;

        if (i >= bpp) {
            left = buffer[i - bpp];
            upper_left = above_buffer[i - bpp];
        }

        buffer[i] = static_cast<unsigned char>(buffer[i] + PaethPredictor(left, up, upper_left));
    }
}

int
Pl_PNGFilter::PaethPredictor(int a, int b, int c)
{
    int p = a + b - c;
    int pa = abs_diff(p, a);
    int pb = abs_diff(p, b);
    int pc = abs_diff(p, c);

    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

void
Pl_PNGFilter::encodeRow()
{
    // For now, hard-code to using UP filter.
    unsigned char ch = 2;
    next()->write(&ch, 1);
    if (prev_row) {
        for (unsigned int i = 0; i < bytes_per_row; ++i) {
            ch = static_cast<unsigned char>(cur_row[i] - prev_row[i]);
            next()->write(&ch, 1);
        }
    } else {
        next()->write(cur_row, bytes_per_row);
    }
}

void
Pl_PNGFilter::finish()
{
    if (pos) {
        // write partial row
        processRow();
    }
    prev_row = nullptr;
    cur_row = buf1.get();
    pos = 0;
    memset(cur_row, 0, bytes_per_row + 1);

    next()->finish();
}

#include <qpdf/Pl_PNGFilter.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <string.h>
#include <limits.h>

static int abs_diff(int a, int b)
{
    return a > b ? a - b : b - a;
}

Pl_PNGFilter::Pl_PNGFilter(char const* identifier, Pipeline* next,
                           action_e action, unsigned int columns,
                           unsigned int samples_per_pixel,
                           unsigned int bits_per_sample) :
    Pipeline(identifier, next),
    action(action),
    cur_row(0),
    prev_row(0),
    buf1(0),
    buf2(0),
    pos(0)
{
    if (samples_per_pixel < 1)
    {
        throw std::runtime_error(
            "PNGFilter created with invalid samples_per_pixel");
    }
    if (! ((bits_per_sample == 1) ||
           (bits_per_sample == 2) ||
           (bits_per_sample == 4) ||
           (bits_per_sample == 8) ||
           (bits_per_sample == 16)))
    {
        throw std::runtime_error(
            "PNGFilter created with invalid bits_per_sample not"
            " 1, 2, 4, 8, or 16");
    }
    this->bytes_per_pixel = ((bits_per_sample * samples_per_pixel) + 7) / 8;
    unsigned long long bpr =
        ((columns * bits_per_sample * samples_per_pixel) + 7) / 8;
    if ((bpr == 0) || (bpr > (UINT_MAX - 1)))
    {
        throw std::runtime_error(
            "PNGFilter created with invalid columns value");
    }
    this->bytes_per_row = bpr & UINT_MAX;
    this->buf1 = make_array_pointer_holder<unsigned char>(
        this->bytes_per_row + 1);
    this->buf2 = make_array_pointer_holder<unsigned char>(
        this->bytes_per_row + 1);
    memset(this->buf1.get(), 0, this->bytes_per_row + 1);
    memset(this->buf2.get(), 0, this->bytes_per_row + 1);
    this->cur_row = this->buf1.get();
    this->prev_row = this->buf2.get();

    // number of bytes per incoming row
    this->incoming = (action == a_encode ?
                      this->bytes_per_row :
                      this->bytes_per_row + 1);
}

Pl_PNGFilter::~Pl_PNGFilter()
{
}

void
Pl_PNGFilter::write(unsigned char* data, size_t len)
{
    size_t left = this->incoming - this->pos;
    size_t offset = 0;
    while (len >= left)
    {
        // finish off current row
        memcpy(this->cur_row + this->pos, data + offset, left);
        offset += left;
        len -= left;

        processRow();

        // Swap rows
        unsigned char* t = this->prev_row;
        this->prev_row = this->cur_row;
        this->cur_row = t ? t : this->buf2.get();
        memset(this->cur_row, 0, this->bytes_per_row + 1);
        left = this->incoming;
        this->pos = 0;
    }
    if (len)
    {
        memcpy(this->cur_row + this->pos, data + offset, len);
    }
    this->pos += len;
}

void
Pl_PNGFilter::processRow()
{
    if (this->action == a_encode)
    {
        encodeRow();
    }
    else
    {
        decodeRow();
    }
}

void
Pl_PNGFilter::decodeRow()
{
    int filter = this->cur_row[0];
    if (this->prev_row)
    {
        switch (filter)
        {
          case 0:
            break;
          case 1:
            this->decodeSub();
            break;
          case 2:
            this->decodeUp();
            break;
          case 3:
            this->decodeAverage();
            break;
          case 4:
            this->decodePaeth();
            break;
          default:
            // ignore
            break;
        }
    }

    getNext()->write(this->cur_row + 1, this->bytes_per_row);
}

void
Pl_PNGFilter::decodeSub()
{
    QTC::TC("libtests", "Pl_PNGFilter decodeSub");
    unsigned char* buffer = this->cur_row + 1;
    unsigned int bpp = this->bytes_per_pixel;

    for (unsigned int i = 0; i < this->bytes_per_row; ++i)
    {
        unsigned char left = 0;

        if (i >= bpp)
        {
            left = buffer[i - bpp];
        }

        buffer[i] = static_cast<unsigned char>(buffer[i] + left);
    }
}

void
Pl_PNGFilter::decodeUp()
{
    QTC::TC("libtests", "Pl_PNGFilter decodeUp");
    unsigned char* buffer = this->cur_row + 1;
    unsigned char* above_buffer = this->prev_row + 1;

    for (unsigned int i = 0; i < this->bytes_per_row; ++i)
    {
        unsigned char up = above_buffer[i];
        buffer[i] = static_cast<unsigned char>(buffer[i] + up);
    }
}

void
Pl_PNGFilter::decodeAverage()
{
    QTC::TC("libtests", "Pl_PNGFilter decodeAverage");
    unsigned char* buffer = this->cur_row + 1;
    unsigned char* above_buffer = this->prev_row + 1;
    unsigned int bpp = this->bytes_per_pixel;

    for (unsigned int i = 0; i < this->bytes_per_row; ++i)
    {
        int left = 0;
        int up = 0;

        if (i >= bpp)
        {
            left = buffer[i - bpp];
        }

        up = above_buffer[i];
        buffer[i] = static_cast<unsigned char>(buffer[i] + (left+up) / 2);
    }
}

void
Pl_PNGFilter::decodePaeth()
{
    QTC::TC("libtests", "Pl_PNGFilter decodePaeth");
    unsigned char* buffer = this->cur_row + 1;
    unsigned char* above_buffer = this->prev_row + 1;
    unsigned int bpp = this->bytes_per_pixel;

    for (unsigned int i = 0; i < this->bytes_per_row; ++i)
    {
        int left = 0;
        int up = above_buffer[i];
        int upper_left = 0;

        if (i >= bpp)
        {
            left = buffer[i - bpp];
            upper_left = above_buffer[i - bpp];
        }

        buffer[i] = static_cast<unsigned char>(
            buffer[i] +
            this->PaethPredictor(left, up, upper_left));
    }
}

int
Pl_PNGFilter::PaethPredictor(int a, int b, int c)
{
    int p = a + b - c;
    int pa = abs_diff(p, a);
    int pb = abs_diff(p, b);
    int pc = abs_diff(p, c);

    if (pa <= pb && pa <= pc)
    {
        return a;
    }
    if (pb <= pc)
    {
        return b;
    }
    return c;
}

void
Pl_PNGFilter::encodeRow()
{
    // For now, hard-code to using UP filter.
    unsigned char ch = 2;
    getNext()->write(&ch, 1);
    if (this->prev_row)
    {
        for (unsigned int i = 0; i < this->bytes_per_row; ++i)
        {
            ch = static_cast<unsigned char>(
                this->cur_row[i] - this->prev_row[i]);
            getNext()->write(&ch, 1);
        }
    }
    else
    {
        getNext()->write(this->cur_row, this->bytes_per_row);
    }
}

void
Pl_PNGFilter::finish()
{
    if (this->pos)
    {
        // write partial row
        processRow();
    }
    this->prev_row = 0;
    this->cur_row = buf1.get();
    this->pos = 0;
    memset(this->cur_row, 0, this->bytes_per_row + 1);

    getNext()->finish();
}

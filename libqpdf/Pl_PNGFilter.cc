#include <qpdf/Pl_PNGFilter.hh>
#include <stdexcept>
#include <string.h>
#include <limits.h>
#include <math.h>

Pl_PNGFilter::Pl_PNGFilter(char const* identifier, Pipeline* next,
               action_e action, unsigned int columns,
               unsigned int bytes_per_pixel) :
    Pipeline(identifier, next),
    action(action),
    columns(columns),
    cur_row(0),
    prev_row(0),
    buf1(0),
    buf2(0),
    bytes_per_pixel(bytes_per_pixel),
    pos(0)
{
    if ((columns == 0) || (columns > UINT_MAX - 1))
    {
        throw std::runtime_error(
            "PNGFilter created with invalid columns value");
    }
    this->buf1 = new unsigned char[columns + 1];
    this->buf2 = new unsigned char[columns + 1];
    this->cur_row = buf1;

    // number of bytes per incoming row
    this->incoming = (action == a_encode ? columns : columns + 1);
}

Pl_PNGFilter::~Pl_PNGFilter()
{
    delete [] buf1;
    delete [] buf2;
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
        this->cur_row = t ? t : this->buf2;
        memset(this->cur_row, 0, this->columns + 1);
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
            case 0:			// none
                break;
            case 1:			// sub
                this->decodeSub();
                break;
            case 2:			// up
                this->decodeUp();
                break;
            case 3:			// average
                this->decodeAverage();
                break;
            case 4:			// Paeth
                this->decodePaeth();
                break;
            default:
                // ignore
                break;
        }
    }

    getNext()->write(this->cur_row + 1, this->columns);
}

void
Pl_PNGFilter::decodeSub()
{
    unsigned char* buffer = this->cur_row + 1;
    unsigned int bpp = this->bytes_per_pixel != 0 ? this->bytes_per_pixel : 1;

    for (unsigned int i = 0; i < this->columns; ++i)
    {
        unsigned char left = 0;

        if (i >= bpp) 
        {
            left = buffer[i - bpp];
        }

        buffer[i] += left;
    }
}

void
Pl_PNGFilter::decodeUp()
{
    unsigned char* buffer = this->cur_row + 1;
    unsigned char* above_buffer = this->prev_row + 1;
    
    for (unsigned int i = 0; i < this->columns; ++i)
    {
        unsigned char up = above_buffer[i];
        buffer[i] += up;
    }
}

void
Pl_PNGFilter::decodeAverage()
{
    unsigned char* buffer = this->cur_row+1;
    unsigned char* above_buffer = this->prev_row+1;
    unsigned int bpp = this->bytes_per_pixel != 0 ? this->bytes_per_pixel : 1;

    for (unsigned int i = 0; i < this->columns; ++i)
    {
        int left = 0, up = 0;
    
        if (i >= bpp)
        {
            left = buffer[i - bpp];
        }

        up = above_buffer[i];
        buffer[i] += floor((left+up) / 2);
    }
}

void
Pl_PNGFilter::decodePaeth()
{
    unsigned char* buffer = this->cur_row+1;
    unsigned char* above_buffer = this->prev_row+1;
    unsigned int bpp = this->bytes_per_pixel != 0 ? this->bytes_per_pixel : 1;

    for (unsigned int i = 0; i < this->columns; ++i)
    {
        int left = 0, 
             up = above_buffer[i], 
             upper_left = 0;
    
        if (i >= bpp)
        {
            left = buffer[i - bpp];
            upper_left = above_buffer[i - bpp];
        }

        buffer[i] += this->PaethPredictor(left, up, upper_left);
    }
}

int 
Pl_PNGFilter::PaethPredictor(int a, int b, int c)
{
    int p = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);

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
    getNext()->write(&ch, 1);
    if (this->prev_row)
    {
        for (unsigned int i = 0; i < this->columns; ++i)
        {
            ch = this->cur_row[i] - this->prev_row[i];
            getNext()->write(&ch, 1);
        }
    }
    else
    {
        getNext()->write(this->cur_row, this->columns);
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
    this->cur_row = buf1;
    this->pos = 0;
    memset(this->cur_row, 0, this->columns + 1);

    getNext()->finish();
}

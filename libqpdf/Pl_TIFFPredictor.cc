#include <qpdf/Pl_TIFFPredictor.hh>

#include <qpdf/BitStream.hh>
#include <qpdf/BitWriter.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <limits.h>
#include <stdexcept>
#include <string.h>
#include <vector>

Pl_TIFFPredictor::Pl_TIFFPredictor(
    char const* identifier,
    Pipeline* next,
    action_e action,
    unsigned int columns,
    unsigned int samples_per_pixel,
    unsigned int bits_per_sample) :
    Pipeline(identifier, next),
    action(action),
    columns(columns),
    samples_per_pixel(samples_per_pixel),
    bits_per_sample(bits_per_sample),
    pos(0)
{
    if (samples_per_pixel < 1) {
        throw std::runtime_error(
            "TIFFPredictor created with invalid samples_per_pixel");
    }
    if ((bits_per_sample < 1) ||
        (bits_per_sample > (8 * (sizeof(unsigned long long))))) {
        throw std::runtime_error(
            "TIFFPredictor created with invalid bits_per_sample");
    }
    unsigned long long bpr =
        ((columns * bits_per_sample * samples_per_pixel) + 7) / 8;
    if ((bpr == 0) || (bpr > (UINT_MAX - 1))) {
        throw std::runtime_error(
            "TIFFPredictor created with invalid columns value");
    }
    this->bytes_per_row = bpr & UINT_MAX;
    this->cur_row =
        make_array_pointer_holder<unsigned char>(this->bytes_per_row);
    memset(this->cur_row.get(), 0, this->bytes_per_row);
}

Pl_TIFFPredictor::~Pl_TIFFPredictor()
{
}

void
Pl_TIFFPredictor::write(unsigned char* data, size_t len)
{
    size_t left = this->bytes_per_row - this->pos;
    size_t offset = 0;
    while (len >= left) {
        // finish off current row
        memcpy(this->cur_row.get() + this->pos, data + offset, left);
        offset += left;
        len -= left;

        processRow();

        // Prepare for next row
        memset(this->cur_row.get(), 0, this->bytes_per_row);
        left = this->bytes_per_row;
        this->pos = 0;
    }
    if (len) {
        memcpy(this->cur_row.get() + this->pos, data + offset, len);
    }
    this->pos += len;
}

void
Pl_TIFFPredictor::processRow()
{
    QTC::TC(
        "libtests",
        "Pl_TIFFPredictor processRow",
        (action == a_decode ? 0 : 1));
    BitWriter bw(this->getNext());
    BitStream in(this->cur_row.get(), this->bytes_per_row);
    std::vector<long long> prev;
    for (unsigned int i = 0; i < this->samples_per_pixel; ++i) {
        long long sample = in.getBitsSigned(this->bits_per_sample);
        bw.writeBitsSigned(sample, this->bits_per_sample);
        prev.push_back(sample);
    }
    for (unsigned int col = 1; col < this->columns; ++col) {
        for (unsigned int i = 0; i < this->samples_per_pixel; ++i) {
            long long sample = in.getBitsSigned(this->bits_per_sample);
            long long new_sample = sample;
            if (action == a_encode) {
                new_sample -= prev[i];
                prev[i] = sample;
            } else {
                new_sample += prev[i];
                prev[i] = new_sample;
            }
            bw.writeBitsSigned(new_sample, this->bits_per_sample);
        }
    }
    bw.flush();
}

void
Pl_TIFFPredictor::finish()
{
    if (this->pos) {
        // write partial row
        processRow();
    }
    this->pos = 0;
    memset(this->cur_row.get(), 0, this->bytes_per_row);
    getNext()->finish();
}

#include <qpdf/Pl_TIFFPredictor.hh>

#include <qpdf/BitStream.hh>
#include <qpdf/BitWriter.hh>
#include <qpdf/QTC.hh>

#include <climits>
#include <stdexcept>
#include <vector>
#include <iostream>

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
    p_next(getNext())
{
    if (samples_per_pixel < 1) {
        throw std::runtime_error("TIFFPredictor created with invalid samples_per_pixel");
    }
    if ((bits_per_sample < 1) || (bits_per_sample > (8 * (sizeof(unsigned long long))))) {
        throw std::runtime_error("TIFFPredictor created with invalid bits_per_sample");
    }
    unsigned long long bpr = ((columns * bits_per_sample * samples_per_pixel) + 7) / 8;
    if ((bpr == 0) || (bpr > (UINT_MAX - 1))) {
        throw std::runtime_error("TIFFPredictor created with invalid columns value");
    }
    this->bytes_per_row = bpr & UINT_MAX;
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
    BitWriter bw(p_next);
    BitStream in(cur_row.data(), cur_row.size());
    previous.clear();
    for (unsigned int i = 0; i < this->samples_per_pixel; ++i) {
        long long sample = in.getBitsSigned(this->bits_per_sample);
        bw.writeBitsSigned(sample, this->bits_per_sample);
        previous.push_back(sample);
    }
    for (unsigned int col = 1; col < this->columns; ++col) {
        for (auto& prev : previous) {
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
    p_next->finish();
}

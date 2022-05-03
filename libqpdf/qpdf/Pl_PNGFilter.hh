#ifndef PL_PNGFILTER_HH
#define PL_PNGFILTER_HH

// This pipeline applies or reverses the application of a PNG filter
// as described in the PNG specification.

// NOTE: In its current implementation, this filter always encodes
// using the "up" filter, but it decodes all the filters.

#include <qpdf/Pipeline.hh>

class Pl_PNGFilter: public Pipeline
{
  public:
    // Encoding is only partially supported
    enum action_e { a_encode, a_decode };

    Pl_PNGFilter(
        char const* identifier,
        Pipeline* next,
        action_e action,
        unsigned int columns,
        unsigned int samples_per_pixel = 1,
        unsigned int bits_per_sample = 8);
    virtual ~Pl_PNGFilter() = default;

    virtual void write(unsigned char const* data, size_t len);
    virtual void finish();

  private:
    void decodeSub();
    void decodeUp();
    void decodeAverage();
    void decodePaeth();
    void processRow();
    void encodeRow();
    void decodeRow();
    int PaethPredictor(int a, int b, int c);

    action_e action;
    unsigned int bytes_per_row;
    unsigned int bytes_per_pixel;
    unsigned char* cur_row;  // points to buf1 or buf2
    unsigned char* prev_row; // points to buf1 or buf2
    std::shared_ptr<unsigned char> buf1;
    std::shared_ptr<unsigned char> buf2;
    size_t pos;
    size_t incoming;
};

#endif // PL_PNGFILTER_HH

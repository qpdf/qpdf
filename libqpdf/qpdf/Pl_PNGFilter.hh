#ifndef __PL_PNGFILTER_HH__
#define __PL_PNGFILTER_HH__

// This pipeline applies or reverses the application of a PNG filter
// as described in the PNG specification.

// NOTE: In its current implementation, this filter always encodes
// using the "up" filter, but it decodes all the filters.

#include <qpdf/Pipeline.hh>

class Pl_PNGFilter: public Pipeline
{
  public:
    // Encoding is not presently supported
    enum action_e { a_encode, a_decode };

    QPDF_DLL
    Pl_PNGFilter(char const* identifier, Pipeline* next,
		 action_e action, unsigned int columns,
		 unsigned int bytes_per_pixel);
    QPDF_DLL
    virtual ~Pl_PNGFilter();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
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
    unsigned int columns;
    unsigned char* cur_row;
    unsigned char* prev_row;
    unsigned char* buf1;
    unsigned char* buf2;
    unsigned int bytes_per_pixel;
    size_t pos;
    size_t incoming;
};

#endif // __PL_PNGFILTER_HH__

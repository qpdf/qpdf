// Read bits from a bit stream.  See BitWriter for writing.

#ifndef __BITSTREAM_HH__
#define __BITSTREAM_HH__

#include <qpdf/DLL.h>

class BitStream
{
  public:
    QPDF_DLL
    BitStream(unsigned char const* p, int nbytes);
    QPDF_DLL
    void reset();
    QPDF_DLL
    unsigned long long getBits(int nbits);
    QPDF_DLL
    void skipToNextByte();

  private:
    unsigned char const* start;
    int nbytes;

    unsigned char const* p;
    unsigned int bit_offset;
    unsigned int bits_available;
};

#endif // __BITSTREAM_HH__

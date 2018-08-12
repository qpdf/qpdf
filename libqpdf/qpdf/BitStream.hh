// Read bits from a bit stream.  See BitWriter for writing.

#ifndef BITSTREAM_HH
#define BITSTREAM_HH

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
    long long getBitsSigned(int nbits);
    QPDF_DLL
    void skipToNextByte();

  private:
    unsigned char const* start;
    int nbytes;

    unsigned char const* p;
    unsigned int bit_offset;
    unsigned int bits_available;
};

#endif // BITSTREAM_HH

// Read bits from a bit stream.  See BitWriter for writing.

#ifndef __BITSTREAM_HH__
#define __BITSTREAM_HH__

#include <qpdf/DLL.hh>

class BitStream
{
  public:
    DLL_EXPORT
    BitStream(unsigned char const* p, int nbytes);
    DLL_EXPORT
    void reset();
    DLL_EXPORT
    unsigned long getBits(int nbits);
    DLL_EXPORT
    void skipToNextByte();

  private:
    unsigned char const* start;
    int nbytes;

    unsigned char const* p;
    unsigned int bit_offset;
    unsigned int bits_available;
};

#endif // __BITSTREAM_HH__

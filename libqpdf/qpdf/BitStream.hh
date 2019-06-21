// Read bits from a bit stream.  See BitWriter for writing.

#ifndef BITSTREAM_HH
#define BITSTREAM_HH

#include <qpdf/DLL.h>
#include <stddef.h>

class BitStream
{
  public:
    QPDF_DLL
    BitStream(unsigned char const* p, size_t nbytes);
    QPDF_DLL
    void reset();
    QPDF_DLL
    unsigned long long getBits(size_t nbits);
    QPDF_DLL
    long long getBitsSigned(size_t nbits);
    // Only call getBitsInt when requesting a number of bits that will
    // definitely fit in an int.
    QPDF_DLL
    int getBitsInt(size_t nbits);
    QPDF_DLL
    void skipToNextByte();

  private:
    unsigned char const* start;
    size_t nbytes;

    unsigned char const* p;
    size_t bit_offset;
    size_t bits_available;
};

#endif // BITSTREAM_HH

// Read bits from a bit stream.  See BitWriter for writing.

#ifndef BITSTREAM_HH
#define BITSTREAM_HH

#include <cstddef>

class BitStream
{
  public:
    BitStream(unsigned char const* p, size_t nbytes);
    void reset();
    unsigned long long getBits(size_t nbits);
    long long getBitsSigned(size_t nbits);
    // Only call getBitsInt when requesting a number of bits that will
    // definitely fit in an int.
    int getBitsInt(size_t nbits);
    void skipToNextByte();

  private:
    unsigned char const* start;
    size_t nbytes;

    unsigned char const* p;
    size_t bit_offset;
    size_t bits_available;
};

#endif // BITSTREAM_HH

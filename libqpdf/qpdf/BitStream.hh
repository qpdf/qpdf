// Read bits from a bit stream.  See BitWriter for writing.

#ifndef __BITSTREAM_HH__
#define __BITSTREAM_HH__

class BitStream
{
  public:
    BitStream(unsigned char const* p, int nbytes);
    void reset();
    unsigned long getBits(int nbits);
    void skipToNextByte();

  private:
    unsigned char const* start;
    int nbytes;

    unsigned char const* p;
    unsigned int bit_offset;
    unsigned int bits_available;
};

#endif // __BITSTREAM_HH__

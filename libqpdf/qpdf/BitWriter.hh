// Write bits into a bit stream.  See BitStream for reading.

#ifndef BITWRITER_HH
#define BITWRITER_HH

#include <cstddef>

class Pipeline;

class BitWriter
{
  public:
    // Write bits to the pipeline.  It is the caller's responsibility to eventually call finish on
    // the pipeline.
    BitWriter(Pipeline* pl);
    void writeBits(unsigned long long val, size_t bits);
    void writeBitsSigned(long long val, size_t bits);
    void writeBitsInt(int val, size_t bits);
    // Force any partial byte to be written to the pipeline.
    void flush();

  private:
    Pipeline* pl;
    unsigned char ch;
    size_t bit_offset;
};

#endif // BITWRITER_HH

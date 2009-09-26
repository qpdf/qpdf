// Write bits into a bit stream.  See BitStream for reading.

#ifndef __BITWRITER_HH__
#define __BITWRITER_HH__

#include <qpdf/DLL.hh>

class Pipeline;

class BitWriter
{
  public:
    // Write bits to the pipeline.  It is the caller's responsibility
    // to eventually call finish on the pipeline.
    DLL_EXPORT
    BitWriter(Pipeline* pl);
    DLL_EXPORT
    void writeBits(unsigned long val, int bits);
    // Force any partial byte to be written to the pipeline.
    DLL_EXPORT
    void flush();

  private:
    Pipeline* pl;
    unsigned char ch;
    unsigned int bit_offset;
};

#endif // __BITWRITER_HH__

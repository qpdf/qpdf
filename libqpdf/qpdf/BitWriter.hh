// Write bits into a bit stream.  See BitStream for reading.

#ifndef BITWRITER_HH
#define BITWRITER_HH

#include <qpdf/DLL.h>
#include <stddef.h>

class Pipeline;

class BitWriter
{
  public:
    // Write bits to the pipeline.  It is the caller's responsibility
    // to eventually call finish on the pipeline.
    QPDF_DLL
    BitWriter(Pipeline* pl);
    QPDF_DLL
    void writeBits(unsigned long long val, size_t bits);
    QPDF_DLL
    void writeBitsSigned(long long val, size_t bits);
    QPDF_DLL
    void writeBitsInt(int val, size_t bits);
    // Force any partial byte to be written to the pipeline.
    QPDF_DLL
    void flush();

  private:
    Pipeline* pl;
    unsigned char ch;
    size_t bit_offset;
};

#endif // BITWRITER_HH

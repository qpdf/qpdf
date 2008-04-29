// Write bits into a bit stream.  See BitStream for reading.

#ifndef __THIS_FILE_Q__
#define __THIS_FILE_Q__

class Pipeline;

class BitWriter
{
  public:
    // Write bits to the pipeline.  It is the caller's responsibility
    // to eventually call finish on the pipeline.
    BitWriter(Pipeline* pl);
    void writeBits(unsigned long val, int bits);
    // Force any partial byte to be written to the pipeline.
    void flush();

  private:
    Pipeline* pl;
    unsigned char ch;
    unsigned int bit_offset;
};

#endif // __THIS_FILE_Q__


#ifndef __PL_ASCIIHEXDECODER_HH__
#define __PL_ASCIIHEXDECODER_HH__

#include <qpdf/Pipeline.hh>

class Pl_ASCIIHexDecoder: public Pipeline
{
  public:
    Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next);
    virtual ~Pl_ASCIIHexDecoder();
    virtual void write(unsigned char* buf, int len);
    virtual void finish();

  private:
    void flush();

    char inbuf[3];
    int pos;
    bool eod;
};

#endif // __PL_ASCIIHEXDECODER_HH__

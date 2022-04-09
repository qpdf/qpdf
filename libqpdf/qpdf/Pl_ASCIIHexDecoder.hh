#ifndef PL_ASCIIHEXDECODER_HH
#define PL_ASCIIHEXDECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCIIHexDecoder: public Pipeline
{
  public:
    Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next);
    virtual ~Pl_ASCIIHexDecoder();
    virtual void write(unsigned char* buf, size_t len);
    virtual void finish();

  private:
    void flush();

    char inbuf[3];
    size_t pos;
    bool eod;
};

#endif // PL_ASCIIHEXDECODER_HH

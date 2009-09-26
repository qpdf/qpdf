
#ifndef __PL_ASCIIHEXDECODER_HH__
#define __PL_ASCIIHEXDECODER_HH__

#include <qpdf/Pipeline.hh>

class Pl_ASCIIHexDecoder: public Pipeline
{
  public:
    DLL_EXPORT
    Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next);
    DLL_EXPORT
    virtual ~Pl_ASCIIHexDecoder();
    DLL_EXPORT
    virtual void write(unsigned char* buf, int len);
    DLL_EXPORT
    virtual void finish();

  private:
    void flush();

    char inbuf[3];
    int pos;
    bool eod;
};

#endif // __PL_ASCIIHEXDECODER_HH__

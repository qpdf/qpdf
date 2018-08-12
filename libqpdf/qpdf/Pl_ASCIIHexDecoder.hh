#ifndef PL_ASCIIHEXDECODER_HH
#define PL_ASCIIHEXDECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCIIHexDecoder: public Pipeline
{
  public:
    QPDF_DLL
    Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next);
    QPDF_DLL
    virtual ~Pl_ASCIIHexDecoder();
    QPDF_DLL
    virtual void write(unsigned char* buf, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    void flush();

    char inbuf[3];
    size_t pos;
    bool eod;
};

#endif // PL_ASCIIHEXDECODER_HH

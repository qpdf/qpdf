#ifndef PL_ASCIIHEXDECODER_HH
#define PL_ASCIIHEXDECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCIIHexDecoder: public Pipeline
{
  public:
    Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next);
    ~Pl_ASCIIHexDecoder() override = default;
    void write(unsigned char const* buf, size_t len) override;
    void finish() override;

  private:
    void flush();

    char inbuf[3];
    size_t pos;
    bool eod;
};

#endif // PL_ASCIIHEXDECODER_HH

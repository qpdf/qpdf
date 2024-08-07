#ifndef PL_ASCIIHEXDECODER_HH
#define PL_ASCIIHEXDECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCIIHexDecoder final: public Pipeline
{
  public:
    Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next);
    ~Pl_ASCIIHexDecoder() final = default;
    void write(unsigned char const* buf, size_t len) final;
    void finish() final;

  private:
    void flush();

    char inbuf[3]{'0', '0', '\0'};
    size_t pos{0};
    bool eod{false};
};

#endif // PL_ASCIIHEXDECODER_HH

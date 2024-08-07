#ifndef PL_ASCII85DECODER_HH
#define PL_ASCII85DECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCII85Decoder final: public Pipeline
{
  public:
    Pl_ASCII85Decoder(char const* identifier, Pipeline* next);
    ~Pl_ASCII85Decoder() final = default;
    void write(unsigned char const* buf, size_t len) final;
    void finish() final;

  private:
    void flush();

    unsigned char inbuf[5]{117, 117, 117, 117, 117};
    size_t pos{0};
    size_t eod{0};
};

#endif // PL_ASCII85DECODER_HH

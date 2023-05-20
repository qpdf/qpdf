#ifndef PL_ASCII85DECODER_HH
#define PL_ASCII85DECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCII85Decoder: public Pipeline
{
  public:
    Pl_ASCII85Decoder(char const* identifier, Pipeline* next);
    ~Pl_ASCII85Decoder() override = default;
    void write(unsigned char const* buf, size_t len) override;
    void finish() override;

  private:
    void flush();

    unsigned char inbuf[5];
    size_t pos;
    size_t eod;
};

#endif // PL_ASCII85DECODER_HH

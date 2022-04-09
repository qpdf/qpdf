#ifndef PL_ASCII85DECODER_HH
#define PL_ASCII85DECODER_HH

#include <qpdf/Pipeline.hh>

class Pl_ASCII85Decoder: public Pipeline
{
  public:
    Pl_ASCII85Decoder(char const* identifier, Pipeline* next);
    virtual ~Pl_ASCII85Decoder();
    virtual void write(unsigned char* buf, size_t len);
    virtual void finish();

  private:
    void flush();

    unsigned char inbuf[5];
    size_t pos;
    size_t eod;
};

#endif // PL_ASCII85DECODER_HH

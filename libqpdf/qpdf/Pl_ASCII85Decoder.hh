
#ifndef __PL_ASCII85DECODER_HH__
#define __PL_ASCII85DECODER_HH__

#include <qpdf/Pipeline.hh>

class Pl_ASCII85Decoder: public Pipeline
{
  public:
    Pl_ASCII85Decoder(char const* identifier, Pipeline* next);
    virtual ~Pl_ASCII85Decoder();
    virtual void write(unsigned char* buf, int len);
    virtual void finish();

  private:
    void flush();

    char inbuf[5];
    int pos;
    int eod;
};

#endif // __PL_ASCII85DECODER_HH__

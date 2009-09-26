
#ifndef __PL_ASCII85DECODER_HH__
#define __PL_ASCII85DECODER_HH__

#include <qpdf/Pipeline.hh>

class Pl_ASCII85Decoder: public Pipeline
{
  public:
    DLL_EXPORT
    Pl_ASCII85Decoder(char const* identifier, Pipeline* next);
    DLL_EXPORT
    virtual ~Pl_ASCII85Decoder();
    DLL_EXPORT
    virtual void write(unsigned char* buf, int len);
    DLL_EXPORT
    virtual void finish();

  private:
    void flush();

    char inbuf[5];
    int pos;
    int eod;
};

#endif // __PL_ASCII85DECODER_HH__

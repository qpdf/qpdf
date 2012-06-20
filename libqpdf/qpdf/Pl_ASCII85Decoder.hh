#ifndef __PL_ASCII85DECODER_HH__
#define __PL_ASCII85DECODER_HH__

#include <qpdf/Pipeline.hh>

class Pl_ASCII85Decoder: public Pipeline
{
  public:
    QPDF_DLL
    Pl_ASCII85Decoder(char const* identifier, Pipeline* next);
    QPDF_DLL
    virtual ~Pl_ASCII85Decoder();
    QPDF_DLL
    virtual void write(unsigned char* buf, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    void flush();

    char inbuf[5];
    size_t pos;
    size_t eod;
};

#endif // __PL_ASCII85DECODER_HH__

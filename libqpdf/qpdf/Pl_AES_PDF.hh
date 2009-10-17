#ifndef __PL_AES_PDF_HH__
#define __PL_AES_PDF_HH__

#include <qpdf/Pipeline.hh>

// This pipeline implements AES-128 with CBC and block padding as
// specified in the PDF specification.

class DLL_EXPORT Pl_AES_PDF: public Pipeline
{
  public:
    // key_data should be a pointer to key_size bytes of data
    static unsigned int const key_size = 16;
    Pl_AES_PDF(char const* identifier, Pipeline* next,
	       bool encrypt, unsigned char key[key_size]);
    virtual ~Pl_AES_PDF();

    virtual void write(unsigned char* data, int len);
    virtual void finish();

  private:
    void flush(bool discard_padding);

    static unsigned int const buf_size = 16;

    bool encrypt;
    unsigned int offset;
    unsigned char key[key_size];
    uint32_t rk[key_size + 28];
    unsigned char inbuf[buf_size];
    unsigned char outbuf[buf_size];
    unsigned int nrounds;
};

#endif // __PL_AES_PDF_HH__

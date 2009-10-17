#ifndef __PL_AES_PDF_HH__
#define __PL_AES_PDF_HH__

#include <qpdf/Pipeline.hh>

class DLL_EXPORT Pl_AES_PDF: public Pipeline
{
  public:
    // key_data should be a pointer to key_size bytes of data
    static unsigned int const key_size = 16;
    Pl_AES_PDF(char const* identifier, Pipeline* next,
	       bool encrypt, unsigned char* key_data);
    virtual ~Pl_AES_PDF();

    virtual void write(unsigned char* data, int len);
    virtual void finish();

  private:
    void flush(bool discard_padding);

    bool encrypt;
    unsigned int offset;
    static unsigned int const buf_size = 16;
    unsigned char buf[buf_size];
};

#endif // __PL_AES_PDF_HH__

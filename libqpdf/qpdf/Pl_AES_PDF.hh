#ifndef __PL_AES_PDF_HH__
#define __PL_AES_PDF_HH__

#include <qpdf/Pipeline.hh>
#include <qpdf/qpdf-config.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

// This pipeline implements AES-128 and AES-256 with CBC and block
// padding as specified in the PDF specification.

class Pl_AES_PDF: public Pipeline
{
  public:
    QPDF_DLL
    // key should be a pointer to key_bytes bytes of data
    Pl_AES_PDF(char const* identifier, Pipeline* next,
	       bool encrypt, unsigned char const* key, unsigned int key_bytes);
    QPDF_DLL
    virtual ~Pl_AES_PDF();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
    virtual void finish();

    // Use zero initialization vector; needed for AESV3
    QPDF_DLL
    void useZeroIV();
    // Disable padding; needed for AESV3
    QPDF_DLL
    void disablePadding();
    // Specify an initialization vector, which will not be included in
    // the output.
    QPDF_DLL
    void setIV(unsigned char const* iv, size_t bytes);

    // For testing only; PDF always uses CBC
    QPDF_DLL
    void disableCBC();
    // For testing only: use a fixed initialization vector for CBC
    QPDF_DLL
    static void useStaticIV();

  private:
    void flush(bool discard_padding);
    void initializeVector();

    static unsigned int const buf_size = 16;
    static bool use_static_iv;

    bool encrypt;
    bool cbc_mode;
    bool first;
    size_t offset;              // offset into memory buffer
    unsigned char* key;
    uint32_t* rk;
    unsigned char inbuf[buf_size];
    unsigned char outbuf[buf_size];
    unsigned char cbc_block[buf_size];
    unsigned char specified_iv[buf_size];
    unsigned int nrounds;
    bool use_zero_iv;
    bool use_specified_iv;
    bool disable_padding;
};

#endif // __PL_AES_PDF_HH__

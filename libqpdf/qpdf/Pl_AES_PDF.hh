#ifndef PL_AES_PDF_HH
#define PL_AES_PDF_HH

#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <memory>

// This pipeline implements AES-128 and AES-256 with CBC and block
// padding as specified in the PDF specification.

class Pl_AES_PDF: public Pipeline
{
  public:
    // key should be a pointer to key_bytes bytes of data
    Pl_AES_PDF(
        char const* identifier,
        Pipeline* next,
        bool encrypt,
        unsigned char const* key,
        size_t key_bytes);
    virtual ~Pl_AES_PDF() = default;

    virtual void write(unsigned char const* data, size_t len);
    virtual void finish();

    // Use zero initialization vector; needed for AESV3
    void useZeroIV();
    // Disable padding; needed for AESV3
    void disablePadding();
    // Specify an initialization vector, which will not be included in
    // the output.
    void setIV(unsigned char const* iv, size_t bytes);

    // For testing only; PDF always uses CBC
    void disableCBC();
    // For testing only: use a fixed initialization vector for CBC
    static void useStaticIV();

  private:
    void flush(bool discard_padding);
    void initializeVector();

    static unsigned int const buf_size = QPDFCryptoImpl::rijndael_buf_size;
    static bool use_static_iv;

    std::shared_ptr<QPDFCryptoImpl> crypto;
    bool encrypt;
    bool cbc_mode;
    bool first;
    size_t offset; // offset into memory buffer
    std::unique_ptr<unsigned char[]> key;
    size_t key_bytes;
    unsigned char inbuf[buf_size];
    unsigned char outbuf[buf_size];
    unsigned char cbc_block[buf_size];
    unsigned char specified_iv[buf_size];
    bool use_zero_iv;
    bool use_specified_iv;
    bool disable_padding;
};

#endif // PL_AES_PDF_HH

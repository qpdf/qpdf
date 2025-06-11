#ifndef QPDFCRYPTO_NATIVE_HH
#define QPDFCRYPTO_NATIVE_HH

#include <qpdf/AES_PDF_native.hh>
#include <qpdf/MD5_native.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/RC4_native.hh>
#include <qpdf/SHA2_native.hh>

#include <memory>

class QPDFCrypto_native final: public QPDFCryptoImpl
{
  public:
    QPDFCrypto_native() = default;

    ~QPDFCrypto_native() final = default;

    void provideRandomData(unsigned char* data, size_t len) final;

    void MD5_init() final;
    void MD5_update(unsigned char const* data, size_t len) final;
    void MD5_finalize() final;
    void MD5_digest(MD5_Digest) final;

    void RC4_init(unsigned char const* key_data, int key_len = -1) final;
    void
    RC4_process(unsigned char const* in_data, size_t len, unsigned char* out_data = nullptr) final;
    void RC4_finalize() final;

    void SHA2_init(int bits) final;
    void SHA2_update(unsigned char const* data, size_t len) final;
    void SHA2_finalize() final;
    std::string SHA2_digest() final;

    void rijndael_init(
        bool encrypt,
        unsigned char const* key_data,
        size_t key_len,
        bool cbc_mode,
        unsigned char* cbc_block) final;
    void rijndael_process(unsigned char* in_data, unsigned char* out_data) final;
    void rijndael_finalize() final;

  private:
    std::shared_ptr<MD5_native> md5;
    std::shared_ptr<RC4_native> rc4;
    std::shared_ptr<SHA2_native> sha2;
    std::shared_ptr<AES_PDF_native> aes_pdf;
};

#endif // QPDFCRYPTO_NATIVE_HH

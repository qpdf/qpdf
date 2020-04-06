#ifndef QPDFCRYPTO_NATIVE_HH
#define QPDFCRYPTO_NATIVE_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/MD5_native.hh>
#include <qpdf/RC4_native.hh>
#include <qpdf/SHA2_native.hh>
#include <qpdf/AES_PDF_native.hh>
#include <memory>

class QPDFCrypto_native: public QPDFCryptoImpl
{
  public:
    QPDFCrypto_native() = default;

    QPDF_DLL
    virtual ~QPDFCrypto_native() = default;

    virtual void provideRandomData(unsigned char* data, size_t len);

    virtual void MD5_init();
    virtual void MD5_update(unsigned char const* data, size_t len);
    virtual void MD5_finalize();
    virtual void MD5_digest(MD5_Digest);

    virtual void RC4_init(unsigned char const* key_data, int key_len = -1);
    virtual void RC4_process(unsigned char* in_data, size_t len,
                             unsigned char* out_data = 0);
    virtual void RC4_finalize();

    virtual void SHA2_init(int bits);
    virtual void SHA2_update(unsigned char const* data, size_t len);
    virtual void SHA2_finalize();
    virtual std::string SHA2_digest();

    virtual void rijndael_init(
        bool encrypt, unsigned char const* key_data, size_t key_len,
        bool cbc_mode, unsigned char* cbc_block);
    virtual void rijndael_process(
        unsigned char* in_data, unsigned char* out_data);
    virtual void rijndael_finalize();

  private:
    std::shared_ptr<MD5_native> md5;
    std::shared_ptr<RC4_native> rc4;
    std::shared_ptr<SHA2_native> sha2;
    std::shared_ptr<AES_PDF_native> aes_pdf;
};

#endif // QPDFCRYPTO_NATIVE_HH

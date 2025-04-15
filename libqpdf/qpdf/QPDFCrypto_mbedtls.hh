#ifndef QPDFCRYPTO_MBEDTLS_HH
#define QPDFCRYPTO_MBEDTLS_HH

#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/RC4_native.hh>

#include <mbedtls/cipher.h>
#include <mbedtls/md.h>
#include <mbedtls/version.h>
#include <memory>

#if MBEDTLS_VERSION_MAJOR == 2
# define QPDF_MBEDTLS_2
#endif

class QPDFCrypto_mbedtls: public QPDFCryptoImpl
{
  public:
    QPDFCrypto_mbedtls();

    virtual ~QPDFCrypto_mbedtls();

    virtual void provideRandomData(unsigned char* data, size_t len);

    virtual void MD5_init();
    virtual void MD5_update(unsigned char const* data, size_t len);
    virtual void MD5_finalize();
    virtual void MD5_digest(MD5_Digest);

    virtual void RC4_init(unsigned char const* key_data, int key_len = -1);
    virtual void RC4_process(unsigned char const* in_data, size_t len, unsigned char* out_data = 0);
    virtual void RC4_finalize();

    virtual void SHA2_init(int bits);
    virtual void SHA2_update(unsigned char const* data, size_t len);
    virtual void SHA2_finalize();
    virtual std::string SHA2_digest();

    virtual void rijndael_init(
        bool encrypt,
        unsigned char const* key_data,
        size_t key_len,
        bool cbc_mode,
        unsigned char* cbc_block);
    virtual void rijndael_process(unsigned char* in_data, unsigned char* out_data);
    virtual void rijndael_finalize();

  private:
    std::shared_ptr<RC4_native> rc4;

    mbedtls_md_context_t md_ctx;
    mbedtls_cipher_context_t cipher_ctx;
    unsigned char md_out[0x40];
    size_t sha2_bits;
};

#endif // QPDFCRYPTO_MBEDTLS_HH

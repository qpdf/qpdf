#ifndef QPDFCRYPTO_OPENSSL_HH
#define QPDFCRYPTO_OPENSSL_HH

#include <qpdf/QPDFCryptoImpl.hh>
#include <string>
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <openssl/opensslv.h>
#if !defined(OPENSSL_VERSION_MAJOR) || OPENSSL_VERSION_MAJOR < 3
# define QPDF_OPENSSL_1
#endif
#include <openssl/rand.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/cipher.h>
#include <openssl/digest.h>
#else
#include <openssl/evp.h>
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif

class QPDFCrypto_openssl: public QPDFCryptoImpl
{
  public:
    QPDFCrypto_openssl();

    QPDF_DLL
    ~QPDFCrypto_openssl() override;

    void provideRandomData(unsigned char* data, size_t len) override;

    void MD5_init() override;
    void MD5_update(unsigned char const* data, size_t len) override;
    void MD5_finalize() override;
    void MD5_digest(MD5_Digest) override;

    void RC4_init(unsigned char const* key_data, int key_len = -1) override;
    void RC4_process(unsigned char* in_data, size_t len,
                     unsigned char* out_data = 0) override;
    void RC4_finalize() override;

    void SHA2_init(int bits) override;
    void SHA2_update(unsigned char const* data, size_t len) override;
    void SHA2_finalize() override;
    std::string SHA2_digest() override;

    void rijndael_init(
        bool encrypt, unsigned char const* key_data, size_t key_len,
        bool cbc_mode, unsigned char* cbc_block) override;
    void rijndael_process(
        unsigned char* in_data, unsigned char* out_data) override;
    void rijndael_finalize() override;

  private:
#ifdef QPDF_OPENSSL_1
    EVP_CIPHER const* rc4;
#else
    OSSL_LIB_CTX* libctx;
    OSSL_PROVIDER* legacy;
    EVP_CIPHER* rc4;
#endif
    EVP_MD_CTX* const md_ctx;
    EVP_CIPHER_CTX* const cipher_ctx;
    uint8_t md_out[EVP_MAX_MD_SIZE];
    size_t sha2_bits;
};

#endif // QPDFCRYPTO_OPENSSL_HH

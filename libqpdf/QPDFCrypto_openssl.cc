#include <qpdf/QPDFCrypto_openssl.hh>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <openssl/err.h>
#ifndef QPDF_OPENSSL_1
# include <openssl/provider.h>
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif

#include <qpdf/QIntC.hh>

#ifndef QPDF_OPENSSL_1
namespace
{
    class RC4Loader
    {
      public:
        static EVP_CIPHER const* getRC4();
        ~RC4Loader();

      private:
        RC4Loader();
        OSSL_PROVIDER* legacy;
        OSSL_LIB_CTX* libctx;
        EVP_CIPHER* rc4;
    };
} // namespace

EVP_CIPHER const*
RC4Loader::getRC4()
{
    static auto loader = std::shared_ptr<RC4Loader>(new RC4Loader());
    return loader->rc4;
}

RC4Loader::RC4Loader()
{
    libctx = OSSL_LIB_CTX_new();
    if (libctx == nullptr) {
        throw std::runtime_error("unable to create openssl library context");
        return;
    }
    legacy = OSSL_PROVIDER_load(libctx, "legacy");
    if (legacy == nullptr) {
        OSSL_LIB_CTX_free(libctx);
        throw std::runtime_error("unable to load openssl legacy provider");
        return;
    }
    rc4 = EVP_CIPHER_fetch(libctx, "RC4", nullptr);
    if (rc4 == nullptr) {
        OSSL_PROVIDER_unload(legacy);
        OSSL_LIB_CTX_free(libctx);
        throw std::runtime_error("unable to load openssl rc4 algorithm");
        return;
    }
}

RC4Loader::~RC4Loader()
{
    EVP_CIPHER_free(rc4);
    OSSL_PROVIDER_unload(legacy);
    OSSL_LIB_CTX_free(libctx);
}
#endif // not QPDF_OPENSSL_1

static void
bad_bits(int bits)
{
    throw std::logic_error(std::string("unsupported key length: ") + std::to_string(bits));
}

static void
check_openssl(int status)
{
    if (status != 1) {
        // OpenSSL creates a "queue" of errors; copy the first (innermost)
        // error to the exception message.
        char buf[256] = "";
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        std::string what = "OpenSSL error: ";
        what += buf;
        throw std::runtime_error(what);
    }
    ERR_clear_error();
}

QPDFCrypto_openssl::QPDFCrypto_openssl() :
    md_ctx(EVP_MD_CTX_new()),
    cipher_ctx(EVP_CIPHER_CTX_new())
{
    memset(md_out, 0, sizeof(md_out));
    EVP_MD_CTX_init(md_ctx);
    EVP_CIPHER_CTX_init(cipher_ctx);
}

QPDFCrypto_openssl::~QPDFCrypto_openssl()
{
    EVP_MD_CTX_reset(md_ctx);
    EVP_CIPHER_CTX_reset(cipher_ctx);
    EVP_CIPHER_CTX_free(cipher_ctx);
    EVP_MD_CTX_free(md_ctx);
}

void
QPDFCrypto_openssl::provideRandomData(unsigned char* data, size_t len)
{
    check_openssl(RAND_bytes(data, QIntC::to_int(len)));
}

void
QPDFCrypto_openssl::MD5_init()
{
    check_openssl(EVP_MD_CTX_reset(md_ctx));
    check_openssl(EVP_DigestInit_ex(md_ctx, EVP_md5(), nullptr));
}

void
QPDFCrypto_openssl::SHA2_init(int bits)
{
    static const EVP_MD* md = EVP_sha512();
    switch (bits) {
    case 256:
        md = EVP_sha256();
        break;
    case 384:
        md = EVP_sha384();
        break;
    case 512:
        md = EVP_sha512();
        break;
    default:
        bad_bits(bits);
        return;
    }
    sha2_bits = static_cast<size_t>(bits);
    check_openssl(EVP_MD_CTX_reset(md_ctx));
    check_openssl(EVP_DigestInit_ex(md_ctx, md, nullptr));
}

void
QPDFCrypto_openssl::MD5_update(unsigned char const* data, size_t len)
{
    check_openssl(EVP_DigestUpdate(md_ctx, data, len));
}

void
QPDFCrypto_openssl::SHA2_update(unsigned char const* data, size_t len)
{
    check_openssl(EVP_DigestUpdate(md_ctx, data, len));
}

void
QPDFCrypto_openssl::MD5_finalize()
{
#ifdef QPDF_OPENSSL_1
    auto md = EVP_MD_CTX_md(md_ctx);
#else
    auto md = EVP_MD_CTX_get0_md(md_ctx);
#endif
    if (md) {
        check_openssl(EVP_DigestFinal(md_ctx, md_out + 0, nullptr));
    }
}

void
QPDFCrypto_openssl::SHA2_finalize()
{
#ifdef QPDF_OPENSSL_1
    auto md = EVP_MD_CTX_md(md_ctx);
#else
    auto md = EVP_MD_CTX_get0_md(md_ctx);
#endif
    if (md) {
        check_openssl(EVP_DigestFinal(md_ctx, md_out + 0, nullptr));
    }
}

void
QPDFCrypto_openssl::MD5_digest(MD5_Digest d)
{
    memcpy(d, md_out, sizeof(QPDFCryptoImpl::MD5_Digest));
}

std::string
QPDFCrypto_openssl::SHA2_digest()
{
    return std::string(reinterpret_cast<char*>(md_out), sha2_bits / 8);
}

void
QPDFCrypto_openssl::RC4_init(unsigned char const* key_data, int key_len)
{
#ifdef QPDF_OPENSSL_1
    static auto const rc4 = EVP_rc4();
#else
    static auto const rc4 = RC4Loader::getRC4();
#endif
    check_openssl(EVP_CIPHER_CTX_reset(cipher_ctx));
    if (key_len == -1) {
        key_len = QIntC::to_int(strlen(reinterpret_cast<const char*>(key_data)));
    }
    check_openssl(EVP_EncryptInit_ex(cipher_ctx, rc4, nullptr, nullptr, nullptr));
    check_openssl(EVP_CIPHER_CTX_set_key_length(cipher_ctx, key_len));
    check_openssl(EVP_EncryptInit_ex(cipher_ctx, nullptr, nullptr, key_data, nullptr));
}

void
QPDFCrypto_openssl::rijndael_init(
    bool encrypt,
    unsigned char const* key_data,
    size_t key_len,
    bool cbc_mode,
    unsigned char* cbc_block)
{
    const EVP_CIPHER* cipher = nullptr;
    switch (key_len) {
    case 32:
        cipher = cbc_mode ? EVP_aes_256_cbc() : EVP_aes_256_ecb();
        break;
    case 24:
        cipher = cbc_mode ? EVP_aes_192_cbc() : EVP_aes_192_ecb();
        break;
    default:
        cipher = cbc_mode ? EVP_aes_128_cbc() : EVP_aes_128_ecb();
        break;
    }

    check_openssl(EVP_CIPHER_CTX_reset(cipher_ctx));
    check_openssl(
        // line-break
        EVP_CipherInit_ex(cipher_ctx, cipher, nullptr, key_data, cbc_block, encrypt));
    check_openssl(EVP_CIPHER_CTX_set_padding(cipher_ctx, 0));
}

void
QPDFCrypto_openssl::RC4_process(unsigned char const* in_data, size_t len, unsigned char* out_data)
{
    int out_len = static_cast<int>(len);
    check_openssl(EVP_EncryptUpdate(cipher_ctx, out_data, &out_len, in_data, out_len));
}

void
QPDFCrypto_openssl::rijndael_process(unsigned char* in_data, unsigned char* out_data)
{
    int len = QPDFCryptoImpl::rijndael_buf_size;
    check_openssl(EVP_CipherUpdate(cipher_ctx, out_data, &len, in_data, len));
}

void
QPDFCrypto_openssl::RC4_finalize()
{
    if (EVP_CIPHER_CTX_cipher(cipher_ctx)) {
        check_openssl(EVP_CIPHER_CTX_reset(cipher_ctx));
    }
}

void
QPDFCrypto_openssl::rijndael_finalize()
{
    if (EVP_CIPHER_CTX_cipher(cipher_ctx)) {
        check_openssl(EVP_CIPHER_CTX_reset(cipher_ctx));
    }
}

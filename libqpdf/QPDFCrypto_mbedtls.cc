#include <qpdf/QPDFCrypto_mbedtls.hh>

#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <stdexcept>

static void
check_mbedtls(int status)
{
    if (status < 0) {
        char err[128];
        mbedtls_strerror(status, err, sizeof(err));
        throw std::runtime_error(std::string("MbedTLS error: ") + err);
    };
}

static void
bad_bits(int bits)
{
    throw std::logic_error(
        std::string("MbedTLS error: unsupported key length ") + std::to_string(bits));
}

QPDFCrypto_mbedtls::QPDFCrypto_mbedtls()
{
    memset(md_out, 0, sizeof(md_out));
    mbedtls_md_init(&md_ctx);
    mbedtls_cipher_init(&cipher_ctx);
}

QPDFCrypto_mbedtls::~QPDFCrypto_mbedtls()
{
    mbedtls_md_free(&md_ctx);
    mbedtls_cipher_free(&cipher_ctx);
}

void
QPDFCrypto_mbedtls::provideRandomData(unsigned char* data, size_t len)
{
    size_t output_len = 0;
    check_mbedtls(mbedtls_platform_entropy_poll(nullptr, data, len, &output_len));
}

void
QPDFCrypto_mbedtls::MD5_init()
{
    mbedtls_md_init(&md_ctx);
    check_mbedtls(mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_MD5), 0));
    check_mbedtls(mbedtls_md_starts(&md_ctx));
}

void
QPDFCrypto_mbedtls::MD5_update(unsigned char const* data, size_t len)
{
    check_mbedtls(mbedtls_md_update(&md_ctx, data, len));
}

void
QPDFCrypto_mbedtls::MD5_finalize()
{
    if (mbedtls_md_get_type(mbedtls_md_info_from_ctx(&md_ctx)) != MBEDTLS_MD_NONE) {
        check_mbedtls(mbedtls_md_finish(&md_ctx, md_out));
        mbedtls_md_free(&md_ctx);
    }
}

void
QPDFCrypto_mbedtls::MD5_digest(MD5_Digest d)
{
    memcpy(d, md_out, sizeof(QPDFCryptoImpl::MD5_Digest));
}

void
QPDFCrypto_mbedtls::RC4_init(unsigned char const* key_data, int key_len)
{
    this->rc4 = std::make_shared<RC4_native>(key_data, key_len);
}

void
QPDFCrypto_mbedtls::RC4_process(unsigned char const* in_data, size_t len, unsigned char* out_data)
{
    this->rc4->process(in_data, len, out_data);
}

void
QPDFCrypto_mbedtls::RC4_finalize()
{
}

void
QPDFCrypto_mbedtls::SHA2_init(int bits)
{
    mbedtls_md_type_t algo = MBEDTLS_MD_NONE;
    switch (bits) {
    case 256:
        algo = MBEDTLS_MD_SHA256;
        break;
    case 384:
        algo = MBEDTLS_MD_SHA384;
        break;
    case 512:
        algo = MBEDTLS_MD_SHA512;
        break;
    default:
        bad_bits(bits);
        return;
    }
    sha2_bits = static_cast<size_t>(bits);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(algo);
    check_mbedtls(mbedtls_md_setup(&md_ctx, info, 0));
    check_mbedtls(mbedtls_md_starts(&md_ctx));
}

void
QPDFCrypto_mbedtls::SHA2_update(unsigned char const* data, size_t len)
{
    check_mbedtls(mbedtls_md_update(&md_ctx, data, len));
}

void
QPDFCrypto_mbedtls::SHA2_finalize()
{
    if (mbedtls_md_get_type(mbedtls_md_info_from_ctx(&md_ctx)) != MBEDTLS_MD_NONE) {
        check_mbedtls(mbedtls_md_finish(&md_ctx, md_out));
        mbedtls_md_free(&md_ctx);
    }
}

std::string
QPDFCrypto_mbedtls::SHA2_digest()
{
    return {reinterpret_cast<char*>(md_out), sha2_bits / 8};
}

void
QPDFCrypto_mbedtls::rijndael_init(
    bool encrypt,
    unsigned char const* key_data,
    size_t key_len,
    bool cbc_mode,
    unsigned char* cbc_block)
{
    const mbedtls_cipher_mode_t mode = cbc_mode ? MBEDTLS_MODE_CBC : MBEDTLS_MODE_ECB;
    const mbedtls_cipher_info_t* info =
        mbedtls_cipher_info_from_values(MBEDTLS_CIPHER_ID_AES, static_cast<int>(key_len * 8), mode);
    const mbedtls_operation_t operation = encrypt ? MBEDTLS_ENCRYPT : MBEDTLS_DECRYPT;

    check_mbedtls(mbedtls_cipher_setup(&cipher_ctx, info));
    check_mbedtls(
        mbedtls_cipher_setkey(&cipher_ctx, key_data, static_cast<int>(key_len * 8), operation));
    check_mbedtls(mbedtls_cipher_set_iv(&cipher_ctx, cbc_block, rijndael_buf_size));
    check_mbedtls(mbedtls_cipher_reset(&cipher_ctx));
}

void
QPDFCrypto_mbedtls::rijndael_process(unsigned char* in_data, unsigned char* out_data)
{
    size_t len = 0;
    check_mbedtls(mbedtls_cipher_update(&cipher_ctx, in_data, rijndael_buf_size, out_data, &len));
}

void
QPDFCrypto_mbedtls::rijndael_finalize()
{
    check_mbedtls(mbedtls_cipher_reset(&cipher_ctx));
}

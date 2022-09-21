#include <qpdf/QPDFCrypto_gnutls.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <cstring>

QPDFCrypto_gnutls::QPDFCrypto_gnutls() :
    hash_ctx(nullptr),
    cipher_ctx(nullptr),
    sha2_bits(0),
    encrypt(false),
    cbc_mode(false),
    aes_key_data(nullptr),
    aes_key_len(0)
{
    memset(digest, 0, sizeof(digest));
}

QPDFCrypto_gnutls::~QPDFCrypto_gnutls()
{
    if (this->hash_ctx) {
        gnutls_hash_deinit(this->hash_ctx, digest);
    }
    if (cipher_ctx) {
        gnutls_cipher_deinit(this->cipher_ctx);
    }
    this->aes_key_data = nullptr;
    this->aes_key_len = 0;
}

void
QPDFCrypto_gnutls::provideRandomData(unsigned char* data, size_t len)
{
    int code = gnutls_rnd(GNUTLS_RND_KEY, data, len);
    if (code < 0) {
        throw std::runtime_error(
            std::string("gnutls: random number generation error: ") +
            std::string(gnutls_strerror(code)));
    }
}

void
QPDFCrypto_gnutls::MD5_init()
{
    MD5_finalize();
    int code = gnutls_hash_init(&this->hash_ctx, GNUTLS_DIG_MD5);
    if (code < 0) {
        this->hash_ctx = nullptr;
        throw std::runtime_error(
            std::string("gnutls: MD5 error: ") +
            std::string(gnutls_strerror(code)));
    }
}

void
QPDFCrypto_gnutls::MD5_update(unsigned char const* data, size_t len)
{
    gnutls_hash(this->hash_ctx, data, len);
}

void
QPDFCrypto_gnutls::MD5_finalize()
{
    if (this->hash_ctx) {
        gnutls_hash_deinit(this->hash_ctx, this->digest);
        this->hash_ctx = nullptr;
    }
}

void
QPDFCrypto_gnutls::MD5_digest(MD5_Digest d)
{
    memcpy(d, this->digest, sizeof(MD5_Digest));
}

void
QPDFCrypto_gnutls::RC4_init(unsigned char const* key_data, int key_len)
{
    RC4_finalize();
    if (key_len == -1) {
        key_len =
            QIntC::to_int(strlen(reinterpret_cast<char const*>(key_data)));
    }
    gnutls_datum_t key;
    key.data = const_cast<unsigned char*>(key_data);
    key.size = QIntC::to_uint(key_len);

    int code = gnutls_cipher_init(
        &this->cipher_ctx, GNUTLS_CIPHER_ARCFOUR_128, &key, nullptr);
    if (code < 0) {
        this->cipher_ctx = nullptr;
        throw std::runtime_error(
            std::string("gnutls: RC4 error: ") +
            std::string(gnutls_strerror(code)));
    }
}

void
QPDFCrypto_gnutls::RC4_process(
    unsigned char const* in_data, size_t len, unsigned char* out_data)
{
    gnutls_cipher_encrypt2(this->cipher_ctx, in_data, len, out_data, len);
}

void
QPDFCrypto_gnutls::RC4_finalize()
{
    if (this->cipher_ctx) {
        gnutls_cipher_deinit(this->cipher_ctx);
        this->cipher_ctx = nullptr;
    }
}

void
QPDFCrypto_gnutls::SHA2_init(int bits)
{
    SHA2_finalize();
    gnutls_digest_algorithm_t alg = GNUTLS_DIG_UNKNOWN;
    switch (bits) {
    case 256:
        alg = GNUTLS_DIG_SHA256;
        break;
    case 384:
        alg = GNUTLS_DIG_SHA384;
        break;
    case 512:
        alg = GNUTLS_DIG_SHA512;
        break;
    default:
        badBits();
        break;
    }
    this->sha2_bits = bits;
    int code = gnutls_hash_init(&this->hash_ctx, alg);
    if (code < 0) {
        this->hash_ctx = nullptr;
        throw std::runtime_error(
            std::string("gnutls: SHA") + std::to_string(bits) +
            " error: " + std::string(gnutls_strerror(code)));
    }
}

void
QPDFCrypto_gnutls::SHA2_update(unsigned char const* data, size_t len)
{
    gnutls_hash(this->hash_ctx, data, len);
}

void
QPDFCrypto_gnutls::SHA2_finalize()
{
    if (this->hash_ctx) {
        gnutls_hash_deinit(this->hash_ctx, this->digest);
        this->hash_ctx = nullptr;
    }
}

std::string
QPDFCrypto_gnutls::SHA2_digest()
{
    std::string result;
    switch (this->sha2_bits) {
    case 256:
        result = std::string(reinterpret_cast<char*>(this->digest), 32);
        break;
    case 384:
        result = std::string(reinterpret_cast<char*>(this->digest), 48);
        break;
    case 512:
        result = std::string(reinterpret_cast<char*>(this->digest), 64);
        break;
    default:
        badBits();
        break;
    }
    return result;
}

void
QPDFCrypto_gnutls::rijndael_init(
    bool encrypt,
    unsigned char const* key_data,
    size_t key_len,
    bool cbc_mode,
    unsigned char* cbc_block)
{
    rijndael_finalize();
    this->encrypt = encrypt;
    this->cbc_mode = cbc_mode;
    if (!cbc_mode) {
        // Save the key so we can re-initialize.
        this->aes_key_data = key_data;
        this->aes_key_len = key_len;
    }

    gnutls_cipher_algorithm_t alg = GNUTLS_CIPHER_UNKNOWN;
    gnutls_datum_t cipher_key;
    gnutls_datum_t iv;

    cipher_key.data = const_cast<unsigned char*>(key_data);

    switch (key_len) {
    case 16:
        alg = GNUTLS_CIPHER_AES_128_CBC;
        break;
    case 32:
        alg = GNUTLS_CIPHER_AES_256_CBC;
        break;
    case 24:
        alg = GNUTLS_CIPHER_AES_192_CBC;
        break;
    default:
        alg = GNUTLS_CIPHER_AES_128_CBC;
        break;
    }

    cipher_key.size = QIntC::to_uint(gnutls_cipher_get_key_size(alg));

    iv.data = cbc_block;
    iv.size = rijndael_buf_size;

    int code = gnutls_cipher_init(&this->cipher_ctx, alg, &cipher_key, &iv);
    if (code < 0) {
        this->cipher_ctx = nullptr;
        throw std::runtime_error(
            std::string("gnutls: AES error: ") +
            std::string(gnutls_strerror(code)));
    }
}

void
QPDFCrypto_gnutls::rijndael_process(
    unsigned char* in_data, unsigned char* out_data)
{
    if (this->encrypt) {
        gnutls_cipher_encrypt2(
            this->cipher_ctx,
            in_data,
            rijndael_buf_size,
            out_data,
            rijndael_buf_size);
    } else {
        gnutls_cipher_decrypt2(
            this->cipher_ctx,
            in_data,
            rijndael_buf_size,
            out_data,
            rijndael_buf_size);
    }

    // Gnutls doesn't support AES in ECB (non-CBC) mode, but the
    // result is the same as if you just reset the cbc block to all
    // zeroes each time. We jump through a few hoops here to make this
    // work.
    if (!this->cbc_mode) {
        static unsigned char zeroes[16] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        rijndael_init(
            this->encrypt,
            this->aes_key_data,
            this->aes_key_len,
            false,
            zeroes);
    }
}

void
QPDFCrypto_gnutls::rijndael_finalize()
{
    if (this->cipher_ctx) {
        gnutls_cipher_deinit(this->cipher_ctx);
        this->cipher_ctx = nullptr;
    }
}

void
QPDFCrypto_gnutls::badBits()
{
    throw std::logic_error("SHA2 (gnutls) has bits != 256, 384, or 512");
}

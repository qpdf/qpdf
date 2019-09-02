#include <cstring>
#include <gnutls/crypto.h>
#include <qpdf/QUtil.hh>
#include <qpdf/rijndael-gnutls.h>

typedef uint32_t u32;

/**
 * Init cryptographic context
 */
void rijndaelInit(gnutls_cipher_hd_t * ctx, unsigned char * key,
  size_t keylen, unsigned char cbc_block[16],
  unsigned int buf_size)
{
  int ret;
  gnutls_cipher_algorithm_t alg;
  gnutls_datum_t cipher_key;
  gnutls_datum_t iv;

  cipher_key.data = key;

  switch(keylen) {
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

  cipher_key.size = gnutls_cipher_get_key_size(alg);

  iv.data = cbc_block;
  iv.size = buf_size;

  ret = gnutls_cipher_init(ctx, alg, &cipher_key, &iv);
  if (ret < 0)
  {
    QUtil::throw_system_error(
      std::string("GNU TLS: AES error: ") + std::string(gnutls_strerror(ret)));
    ctx = nullptr;
    return;
  }
}

/**
 * Encrypt string by AES CBC by GNU TLS.
 */
void rijndaelCBCEncrypt(gnutls_cipher_hd_t ctx,
  const unsigned char plaintext[16], unsigned char ciphertext[16],
  unsigned int buf_size)
{
  if (ctx != nullptr)
    gnutls_cipher_encrypt2(ctx, plaintext, buf_size, ciphertext, buf_size);
}

/**
 * Decrypt string by AES CBC by GNU TLS.
 */
void rijndaelCBCDecrypt(gnutls_cipher_hd_t ctx,
  const unsigned char ciphertext[16], unsigned char plaintext[16],
  unsigned int buf_size)
{
  if (ctx != nullptr)
    gnutls_cipher_decrypt2(ctx, ciphertext, buf_size, plaintext, buf_size);
}

/**
 * Encrypt string by AES ECB by GNU TLS.
 */
void rijndaelECBEncrypt(unsigned char * key, size_t keylen,
  unsigned char cbc_block[16], const unsigned char ciphertext[16],
  unsigned char plaintext[16], unsigned int buf_size)
{
  gnutls_cipher_hd_t ctx;

  rijndaelInit(&ctx, key, keylen, cbc_block, buf_size);

  rijndaelCBCEncrypt(ctx, ciphertext, plaintext, buf_size);

  rijndaelFinish(ctx);
}

/**
 * Decrypt string by AES ECB by GNU TLS.
 */
void rijndaelECBDecrypt(unsigned char * key, size_t keylen,
  unsigned char cbc_block[16], const unsigned char ciphertext[16],
  unsigned char plaintext[16], unsigned int buf_size)
{
  gnutls_cipher_hd_t ctx;

  rijndaelInit(&ctx, key, keylen, cbc_block, buf_size);

  rijndaelCBCDecrypt(ctx, ciphertext, plaintext, buf_size);

  rijndaelFinish(ctx);
}

/**
 * Finish cryptography context
 */
void rijndaelFinish(gnutls_cipher_hd_t ctx)
{
  if (ctx != nullptr)
  {
    gnutls_cipher_deinit(ctx);
    ctx = nullptr;
  }
}

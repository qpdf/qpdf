#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include <gnutls/crypto.h>

void rijndaelInit(gnutls_cipher_hd_t * context, unsigned char * key,
  size_t keylen, unsigned char cbc_block[16], unsigned int buf_size);
int rijndaelSetupEncrypt(uint32_t *rk, const unsigned char *key,
  int keybits);
int rijndaelSetupDecrypt(uint32_t *rk, const unsigned char *key,
  int keybits);
void rijndaelCBCEncrypt(gnutls_cipher_hd_t context,
  const unsigned char plaintext[16], unsigned char ciphertext[16],
  unsigned int buf_size);
void rijndaelCBCDecrypt(gnutls_cipher_hd_t context,
  const unsigned char ciphertext[16], unsigned char plaintext[16],
  unsigned int buf_size);
void rijndaelECBEncrypt(unsigned char * key, size_t keylen,
  unsigned char cbc_block[16], const unsigned char plaintext[16],
  unsigned char ciphertext[16], unsigned int buf_size);
void rijndaelECBDecrypt(unsigned char * key, size_t keylen,
  unsigned char cbc_block[16], const unsigned char ciphertext[16],
  unsigned char plaintext[16], unsigned int buf_size);
void rijndaelFinish(gnutls_cipher_hd_t context);

#define KEYLENGTH(keybits) ((keybits)/8)
#define RKLENGTH(keybits)  ((keybits)/8+28)
#define NROUNDS(keybits)   ((keybits)/32+6)

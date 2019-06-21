#ifndef RIJNDAEL_H
#define RIJNDAEL_H

#include <qpdf/qpdf-config.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stddef.h>

unsigned int rijndaelSetupEncrypt(uint32_t *rk, const unsigned char *key,
  size_t keybits);
unsigned int rijndaelSetupDecrypt(uint32_t *rk, const unsigned char *key,
  size_t keybits);
void rijndaelEncrypt(const uint32_t *rk, unsigned int nrounds,
  const unsigned char plaintext[16], unsigned char ciphertext[16]);
void rijndaelDecrypt(const uint32_t *rk, unsigned int nrounds,
  const unsigned char ciphertext[16], unsigned char plaintext[16]);

#define KEYLENGTH(keybits) ((keybits)/8)
#define RKLENGTH(keybits)  ((keybits)/8+28)
#define NROUNDS(keybits)   ((keybits)/32+6)

#endif

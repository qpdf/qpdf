#ifndef RIJNDAEL_H
#define RIJNDAEL_H

#include <qpdf/qpdf-config.h>
#include <cstdint>
#include <cstddef>

unsigned int rijndaelSetupEncrypt(uint32_t* rk, const unsigned char* key, size_t keybits);
unsigned int rijndaelSetupDecrypt(uint32_t* rk, const unsigned char* key, size_t keybits);
void rijndaelEncrypt(
    const uint32_t* rk,
    unsigned int nrounds,
    const unsigned char plaintext[16],
    unsigned char ciphertext[16]);
void rijndaelDecrypt(
    const uint32_t* rk,
    unsigned int nrounds,
    const unsigned char ciphertext[16],
    unsigned char plaintext[16]);

#define KEYLENGTH(keybits) ((keybits) / 8)
#define RKLENGTH(keybits) ((keybits) / 8 + 28)
#define NROUNDS(keybits) ((keybits) / 32 + 6)

#endif

#include <qpdf/AES_PDF_native.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/rijndael.h>
#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdlib.h>
#include <string>

AES_PDF_native::AES_PDF_native(
    bool encrypt,
    unsigned char const* key,
    size_t key_bytes,
    bool cbc_mode,
    unsigned char* cbc_block) :
    encrypt(encrypt),
    cbc_mode(cbc_mode),
    cbc_block(cbc_block),
    nrounds(0)
{
    size_t keybits = 8 * key_bytes;
    this->key = std::make_unique<unsigned char[]>(key_bytes);
    this->rk = std::make_unique<uint32_t[]>(RKLENGTH(keybits));
    size_t rk_bytes = RKLENGTH(keybits) * sizeof(uint32_t);
    std::memcpy(this->key.get(), key, key_bytes);
    std::memset(this->rk.get(), 0, rk_bytes);
    if (encrypt) {
        this->nrounds =
            rijndaelSetupEncrypt(this->rk.get(), this->key.get(), keybits);
    } else {
        this->nrounds =
            rijndaelSetupDecrypt(this->rk.get(), this->key.get(), keybits);
    }
}

AES_PDF_native::~AES_PDF_native()
{
}

void
AES_PDF_native::update(unsigned char* in_data, unsigned char* out_data)
{
    if (this->encrypt) {
        if (this->cbc_mode) {
            for (size_t i = 0; i < QPDFCryptoImpl::rijndael_buf_size; ++i) {
                in_data[i] ^= this->cbc_block[i];
            }
        }
        rijndaelEncrypt(this->rk.get(), this->nrounds, in_data, out_data);
        if (this->cbc_mode) {
            memcpy(
                this->cbc_block, out_data, QPDFCryptoImpl::rijndael_buf_size);
        }
    } else {
        rijndaelDecrypt(this->rk.get(), this->nrounds, in_data, out_data);
        if (this->cbc_mode) {
            for (size_t i = 0; i < QPDFCryptoImpl::rijndael_buf_size; ++i) {
                out_data[i] ^= this->cbc_block[i];
            }
            memcpy(this->cbc_block, in_data, QPDFCryptoImpl::rijndael_buf_size);
        }
    }
}

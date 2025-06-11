#include <qpdf/AES_PDF_native.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/rijndael.h>
#include <cstring>
#include <stdexcept>
#include <stdlib.h>
#include <string>

AES_PDF_native::AES_PDF_native(
    bool encrypt,
    unsigned char const* a_key,
    size_t key_bytes,
    bool cbc_mode,
    unsigned char* cbc_block) :
    encrypt(encrypt),
    cbc_mode(cbc_mode),
    cbc_block(cbc_block)
{
    size_t keybits = 8 * key_bytes;
    key = std::make_unique<unsigned char[]>(key_bytes);
    rk = std::make_unique<uint32_t[]>(RKLENGTH(keybits));
    size_t rk_bytes = RKLENGTH(keybits) * sizeof(uint32_t);
    std::memcpy(key.get(), a_key, key_bytes);
    std::memset(rk.get(), 0, rk_bytes);
    if (encrypt) {
        nrounds = rijndaelSetupEncrypt(rk.get(), key.get(), keybits);
    } else {
        nrounds = rijndaelSetupDecrypt(rk.get(), key.get(), keybits);
    }
}

void
AES_PDF_native::update(unsigned char* in_data, unsigned char* out_data)
{
    if (encrypt) {
        if (cbc_mode) {
            for (size_t i = 0; i < QPDFCryptoImpl::rijndael_buf_size; ++i) {
                in_data[i] ^= cbc_block[i];
            }
        }
        rijndaelEncrypt(rk.get(), nrounds, in_data, out_data);
        if (cbc_mode) {
            memcpy(cbc_block, out_data, QPDFCryptoImpl::rijndael_buf_size);
        }
    } else {
        rijndaelDecrypt(rk.get(), nrounds, in_data, out_data);
        if (cbc_mode) {
            for (size_t i = 0; i < QPDFCryptoImpl::rijndael_buf_size; ++i) {
                out_data[i] ^= cbc_block[i];
            }
            memcpy(cbc_block, in_data, QPDFCryptoImpl::rijndael_buf_size);
        }
    }
}

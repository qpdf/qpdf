#include <qpdf/AES_PDF_native.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <assert.h>
#include <stdexcept>
#include <qpdf/rijndael.h>
#include <qpdf/QIntC.hh>
#include <string>
#include <stdlib.h>

AES_PDF_native::AES_PDF_native(bool encrypt, unsigned char const* key,
                               size_t key_bytes) :
    encrypt(encrypt),
    nrounds(0)
{
    size_t keybits = 8 * key_bytes;
    this->key = std::unique_ptr<unsigned char[]>(
        new unsigned char[key_bytes],
        std::default_delete<unsigned char[]>());
    this->rk = std::unique_ptr<uint32_t[]>(
        new uint32_t[RKLENGTH(keybits)],
        std::default_delete<uint32_t[]>());
    size_t rk_bytes = RKLENGTH(keybits) * sizeof(uint32_t);
    std::memcpy(this->key.get(), key, key_bytes);
    std::memset(this->rk.get(), 0, rk_bytes);
    if (encrypt)
    {
	this->nrounds = rijndaelSetupEncrypt(
            this->rk.get(), this->key.get(), keybits);
    }
    else
    {
	this->nrounds = rijndaelSetupDecrypt(
            this->rk.get(), this->key.get(), keybits);
    }
}

AES_PDF_native::~AES_PDF_native()
{
}

void
AES_PDF_native::update(unsigned char* in_data, unsigned char* out_data)
{
    if (this->encrypt)
    {
	rijndaelEncrypt(this->rk.get(),
                        this->nrounds, in_data, out_data);
    }
    else
    {
	rijndaelDecrypt(this->rk.get(),
                        this->nrounds, in_data, out_data);
    }
}

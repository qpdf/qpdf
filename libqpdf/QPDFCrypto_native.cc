#include <qpdf/QPDFCrypto_native.hh>
#include <qpdf/QUtil.hh>

void
QPDFCrypto_native::MD5_init()
{
    this->md5 = std::make_shared<MD5_native>();
}

void
QPDFCrypto_native::MD5_update(unsigned char const* data, size_t len)
{
    this->md5->update(const_cast<unsigned char*>(data), len);
}

void
QPDFCrypto_native::MD5_finalize()
{
    this->md5->finalize();
}

void
QPDFCrypto_native::MD5_digest(MD5_Digest d)
{
    this->md5->digest(d);
}

void
QPDFCrypto_native::RC4_init(unsigned char const* key_data, int key_len)
{
    this->rc4 = std::make_shared<RC4_native>(key_data, key_len);
}

void
QPDFCrypto_native::RC4_process(unsigned char* in_data, size_t len,
                               unsigned char* out_data)
{
    this->rc4->process(in_data, len, out_data);
}

void
QPDFCrypto_native::RC4_finalize()
{
}

void
QPDFCrypto_native::SHA2_init(int bits)
{
    this->sha2 = std::make_shared<SHA2_native>(bits);
}

void
QPDFCrypto_native::SHA2_update(unsigned char const* data, size_t len)
{
    this->sha2->update(data, len);
}

void
QPDFCrypto_native::SHA2_finalize()
{
    this->sha2->finalize();
}

std::string
QPDFCrypto_native::SHA2_digest()
{
    return this->sha2->getRawDigest();
}

void
QPDFCrypto_native::rijndael_init(
    bool encrypt, unsigned char const* key_data, size_t key_len,
    bool cbc_mode, unsigned char* cbc_block)

{
    this->aes_pdf = std::make_shared<AES_PDF_native>(
        encrypt, key_data, key_len, cbc_mode, cbc_block);
}

void
QPDFCrypto_native::rijndael_process(unsigned char* in_data,
                                    unsigned char* out_data)
{
    this->aes_pdf->update(in_data, out_data);
}

void
QPDFCrypto_native::rijndael_finalize()
{
}

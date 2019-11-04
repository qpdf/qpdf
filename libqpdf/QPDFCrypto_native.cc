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


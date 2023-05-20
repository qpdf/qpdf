#include <qpdf/RC4.hh>

#include <qpdf/QPDFCryptoProvider.hh>


RC4::RC4(unsigned char const* key_data, int key_len) :
    crypto(QPDFCryptoProvider::getImpl())
{
    this->crypto->RC4_init(key_data, key_len);
}

void
RC4::process(unsigned char const* in_data, size_t len, unsigned char* out_data)
{
    this->crypto->RC4_process(in_data, len, out_data);
}

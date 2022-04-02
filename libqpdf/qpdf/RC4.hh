#ifndef RC4_HH
#define RC4_HH

#include <qpdf/QPDFCryptoImpl.hh>
#include <cstring>
#include <memory>

class RC4
{
  public:
    // key_len of -1 means treat key_data as a null-terminated string
    QPDF_DLL
    RC4(unsigned char const* key_data, int key_len = -1);

    // out_data = 0 means to encrypt/decrypt in place
    QPDF_DLL
    void
    process(unsigned char* in_data, size_t len, unsigned char* out_data = 0);

  private:
    std::shared_ptr<QPDFCryptoImpl> crypto;
};

#endif // RC4_HH

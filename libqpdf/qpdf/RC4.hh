#ifndef RC4_HH
#define RC4_HH

#include <qpdf/QPDFCryptoImpl.hh>
#include <cstring>
#include <memory>

class RC4
{
  public:
    // key_len of -1 means treat key_data as a null-terminated string
    RC4(unsigned char const* key_data, int key_len = -1);

    // It is safe to pass the same pointer to in_data and out_data to encrypt/decrypt in place
    void process(unsigned char const* in_data, size_t len, unsigned char* out_data);

    static void process(std::string_view key, std::string& data);

  private:
    std::shared_ptr<QPDFCryptoImpl> crypto;
};

#endif // RC4_HH

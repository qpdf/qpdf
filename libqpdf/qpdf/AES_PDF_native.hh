#ifndef AES_PDF_NATIVE_HH
#define AES_PDF_NATIVE_HH

#include <cstdint>
#include <cstring>
#include <memory>

class AES_PDF_native
{
  public:
    // key should be a pointer to key_bytes bytes of data
    AES_PDF_native(bool encrypt, unsigned char const* key,
                   size_t key_bytes);
    ~AES_PDF_native();

    void update(unsigned char* in_data, unsigned char* out_data);

  private:
    bool encrypt;
    std::unique_ptr<unsigned char[]> key;
    std::unique_ptr<uint32_t[]> rk;
    unsigned int nrounds;
};

#endif // AES_PDF_NATIVE_HH

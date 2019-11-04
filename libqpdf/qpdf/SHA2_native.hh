#ifndef SHA2_NATIVE_HH
#define SHA2_NATIVE_HH

#include <sph/sph_sha2.h>
#include <string>

class SHA2_native
{
  public:
    SHA2_native(int bits);
    ~SHA2_native() = default;
    void update(unsigned char const* const, size_t);
    void finalize();
    std::string getRawDigest();

  private:
    void badBits();

    int bits;
    sph_sha256_context ctx256;
    sph_sha384_context ctx384;
    sph_sha512_context ctx512;
    unsigned char sha256sum[32];
    unsigned char sha384sum[48];
    unsigned char sha512sum[64];
};

#endif // SHA2_NATIVE_HH

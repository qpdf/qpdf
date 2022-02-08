#ifndef MD5_NATIVE_HH
#define MD5_NATIVE_HH

#include <qpdf/qpdf-config.h>
#include <cstdint>
#include <cstring>

class MD5_native
{
  public:
    typedef unsigned char Digest[16];

    MD5_native();
    void init();
    void update(unsigned char *, size_t);
    void finalize();
    void digest(Digest);

  private:
    static void transform(uint32_t[4], unsigned char[64]);
    static void encode(unsigned char *, uint32_t *, size_t);
    static void decode(uint32_t *, unsigned char *, size_t);

    uint32_t state[4];          // state (ABCD)
    uint32_t count[2];          // number of bits, modulo 2^64 (lsb first)
    unsigned char buffer[64];   // input buffer

    bool finalized;
    Digest digest_val;
};

#endif // MD5_NATIVE_HH

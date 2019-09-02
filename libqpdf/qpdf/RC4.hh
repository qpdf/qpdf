#ifndef RC4_HH
#define RC4_HH

#include <stddef.h>

#include <qpdf/qpdf-config.h>

#ifdef HAVE_GNUTLS
# include <gnutls/crypto.h>
#endif

class RC4
{
  public:
    // key_len of -1 means treat key_data as a null-terminated string
    RC4(unsigned char const* key_data, int key_len = -1);

    // out_data = 0 means to encrypt/decrypt in place
    void process(unsigned char* in_data, size_t len,
                 unsigned char* out_data = 0);

#ifdef HAVE_GNUTLS
    ~RC4();
#endif

  private:
    class RC4Key
    {
      public:
        unsigned char state[256];
        unsigned char x;
        unsigned char y;
    };

    RC4Key key;


#ifdef HAVE_GNUTLS
    gnutls_cipher_hd_t ctx;
#endif
};

#endif // RC4_HH

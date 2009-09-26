#ifndef __RC4_HH__
#define __RC4_HH__

class RC4
{
  public:
    // key_len of -1 means treat key_data as a null-terminated string
    RC4(unsigned char const* key_data, int key_len = -1);

    // out_data = 0 means to encrypt/decrypt in place
    void process(unsigned char* in_data, int len, unsigned char* out_data = 0);

  private:
    class RC4Key
    {
      public:
        unsigned char state[256];
        unsigned char x;
        unsigned char y;
    };

    RC4Key key;
};

#endif // __RC4_HH__

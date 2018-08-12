#ifndef RC4_HH
#define RC4_HH

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

#endif // RC4_HH

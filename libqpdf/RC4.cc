#include <qpdf/RC4.hh>

#include <string.h>

static void swap_byte(unsigned char &a, unsigned char &b)
{
    unsigned char t;

    t = a;
    a = b;
    b = t;
}

RC4::RC4(unsigned char const* key_data, int key_len)
{
    if (key_len == -1)
    {
	key_len = strlen(reinterpret_cast<char const*>(key_data));
    }

    for (int i = 0; i < 256; ++i)
    {
        key.state[i] = i;
    }
    key.x = 0;
    key.y = 0;

    int i1 = 0;
    int i2 = 0;
    for (int i = 0; i < 256; ++i)
    {
	i2 = (key_data[i1] + key.state[i] + i2) % 256;
	swap_byte(key.state[i], key.state[i2]);
	i1 = (i1 + 1) % key_len;
    }
}

void
RC4::process(unsigned char *in_data, int len, unsigned char* out_data)
{
    if (out_data == 0)
    {
	// Convert in place
	out_data = in_data;
    }

    for (int i = 0; i < len; ++i)
    {
	key.x = (key.x + 1) % 256;
	key.y = (key.state[key.x] + key.y) % 256;
	swap_byte(key.state[key.x], key.state[key.y]);
	int xor_index = (key.state[key.x] + key.state[key.y]) % 256;
	out_data[i] = in_data[i] ^ key.state[xor_index];
    }
}

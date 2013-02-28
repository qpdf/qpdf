// This file implements a class for computation of MD5 checksums.
// It is derived from the reference algorithm for MD5 as given in
// RFC 1321.  The original copyright notice is as follows:
//
/////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
// rights reserved.
//
// License to copy and use this software is granted provided that it
// is identified as the "RSA Data Security, Inc. MD5 Message-Digest
// Algorithm" in all material mentioning or referencing this software
// or this function.
//
// License is also granted to make and use derivative works provided
// that such works are identified as "derived from the RSA Data
// Security, Inc. MD5 Message-Digest Algorithm" in all material
// mentioning or referencing the derived work.
//
// RSA Data Security, Inc. makes no representations concerning either
// the merchantability of this software or the suitability of this
// software for any particular purpose. It is provided "as is"
// without express or implied warranty of any kind.
//
// These notices must be retained in any copies of any part of this
// documentation and/or software.
//
/////////////////////////////////////////////////////////////////////////

#include <qpdf/MD5.hh>
#include <qpdf/QUtil.hh>

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int const S11 = 7;
int const S12 = 12;
int const S13 = 17;
int const S14 = 22;
int const S21 = 5;
int const S22 = 9;
int const S23 = 14;
int const S24 = 20;
int const S31 = 4;
int const S32 = 11;
int const S33 = 16;
int const S34 = 23;
int const S41 = 6;
int const S42 = 10;
int const S43 = 15;
int const S44 = 21;

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// F, G, H and I are basic MD5 functions.
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

// ROTATE_LEFT rotates x left n bits.
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
// Rotation is separate from addition to prevent recomputation.
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + static_cast<UINT4>(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + static_cast<UINT4>(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + static_cast<UINT4>(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + static_cast<UINT4>(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

// MD5 initialization. Begins an MD5 operation, writing a new context.
void MD5::init()
{
    count[0] = count[1] = 0;
    // Load magic initialization constants.
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;

    finalized = false;
    memset(digest_val, 0, sizeof(digest_val));
}

// MD5 block update operation. Continues an MD5 message-digest
// operation, processing another message block, and updating the
// context.

void MD5::update(unsigned char *input,
		 unsigned int inputLen)
{
    unsigned int i, index, partLen;

    // Compute number of bytes mod 64
    index = static_cast<unsigned int>((count[0] >> 3) & 0x3f);

    // Update number of bits
    if ((count[0] += (static_cast<UINT4>(inputLen) << 3)) <
        (static_cast<UINT4>(inputLen) << 3))
	count[1]++;
    count[1] += (static_cast<UINT4>(inputLen) >> 29);

    partLen = 64 - index;

    // Transform as many times as possible.

    if (inputLen >= partLen) {
	memcpy(&buffer[index], input, partLen);
	transform(state, buffer);

	for (i = partLen; i + 63 < inputLen; i += 64)
	    transform(state, &input[i]);

	index = 0;
    }
    else
	i = 0;

    // Buffer remaining input
    memcpy(&buffer[index], &input[i], inputLen-i);
}

// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
void MD5::final()
{
    if (finalized)
    {
	return;
    }

    unsigned char bits[8];
    unsigned int index, padLen;

    // Save number of bits
    encode(bits, count, 8);

    // Pad out to 56 mod 64.

    index = static_cast<unsigned int>((count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update(PADDING, padLen);

    // Append length (before padding)
    update(bits, 8);
    // Store state in digest_val
    encode(digest_val, state, 16);

    // Zeroize sensitive information.
    memset(state, 0, sizeof(state));
    memset(count, 0, sizeof(count));
    memset(buffer, 0, sizeof(buffer));

    finalized = true;
}

// MD5 basic transformation. Transforms state based on block.
void MD5::transform(UINT4 state[4], unsigned char block[64])
{
    UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    decode(x, block, 64);

    // Round 1
    FF (a, b, c, d, x[ 0], S11, 0xd76aa478); // 1
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); // 2
    FF (c, d, a, b, x[ 2], S13, 0x242070db); // 3
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); // 4
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); // 5
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a); // 6
    FF (c, d, a, b, x[ 6], S13, 0xa8304613); // 7
    FF (b, c, d, a, x[ 7], S14, 0xfd469501); // 8
    FF (a, b, c, d, x[ 8], S11, 0x698098d8); // 9
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); // 10
    FF (c, d, a, b, x[10], S13, 0xffff5bb1); // 11
    FF (b, c, d, a, x[11], S14, 0x895cd7be); // 12
    FF (a, b, c, d, x[12], S11, 0x6b901122); // 13
    FF (d, a, b, c, x[13], S12, 0xfd987193); // 14
    FF (c, d, a, b, x[14], S13, 0xa679438e); // 15
    FF (b, c, d, a, x[15], S14, 0x49b40821); // 16

    // Round 2
    GG (a, b, c, d, x[ 1], S21, 0xf61e2562); // 17
    GG (d, a, b, c, x[ 6], S22, 0xc040b340); // 18
    GG (c, d, a, b, x[11], S23, 0x265e5a51); // 19
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); // 20
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d); // 21
    GG (d, a, b, c, x[10], S22,  0x2441453); // 22
    GG (c, d, a, b, x[15], S23, 0xd8a1e681); // 23
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); // 24
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); // 25
    GG (d, a, b, c, x[14], S22, 0xc33707d6); // 26
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); // 27
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed); // 28
    GG (a, b, c, d, x[13], S21, 0xa9e3e905); // 29
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); // 30
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9); // 31
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); // 32

    // Round 3
    HH (a, b, c, d, x[ 5], S31, 0xfffa3942); // 33
    HH (d, a, b, c, x[ 8], S32, 0x8771f681); // 34
    HH (c, d, a, b, x[11], S33, 0x6d9d6122); // 35
    HH (b, c, d, a, x[14], S34, 0xfde5380c); // 36
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44); // 37
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); // 38
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); // 39
    HH (b, c, d, a, x[10], S34, 0xbebfbc70); // 40
    HH (a, b, c, d, x[13], S31, 0x289b7ec6); // 41
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); // 42
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); // 43
    HH (b, c, d, a, x[ 6], S34,  0x4881d05); // 44
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); // 45
    HH (d, a, b, c, x[12], S32, 0xe6db99e5); // 46
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8); // 47
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); // 48

    // Round 4
    II (a, b, c, d, x[ 0], S41, 0xf4292244); // 49
    II (d, a, b, c, x[ 7], S42, 0x432aff97); // 50
    II (c, d, a, b, x[14], S43, 0xab9423a7); // 51
    II (b, c, d, a, x[ 5], S44, 0xfc93a039); // 52
    II (a, b, c, d, x[12], S41, 0x655b59c3); // 53
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); // 54
    II (c, d, a, b, x[10], S43, 0xffeff47d); // 55
    II (b, c, d, a, x[ 1], S44, 0x85845dd1); // 56
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); // 57
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0); // 58
    II (c, d, a, b, x[ 6], S43, 0xa3014314); // 59
    II (b, c, d, a, x[13], S44, 0x4e0811a1); // 60
    II (a, b, c, d, x[ 4], S41, 0xf7537e82); // 61
    II (d, a, b, c, x[11], S42, 0xbd3af235); // 62
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); // 63
    II (b, c, d, a, x[ 9], S44, 0xeb86d391); // 64

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    // Zeroize sensitive information.

    memset (x, 0, sizeof (x));
}

// Encodes input (UINT4) into output (unsigned char). Assumes len is a
// multiple of 4.
void MD5::encode(unsigned char *output, UINT4 *input, unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
	output[j] = static_cast<unsigned char>(input[i] & 0xff);
	output[j+1] = static_cast<unsigned char>((input[i] >> 8) & 0xff);
	output[j+2] = static_cast<unsigned char>((input[i] >> 16) & 0xff);
	output[j+3] = static_cast<unsigned char>((input[i] >> 24) & 0xff);
    }
}

// Decodes input (unsigned char) into output (UINT4). Assumes len is a
// multiple of 4.
void MD5::decode(UINT4 *output, unsigned char *input, unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
	output[i] =
            static_cast<UINT4>(input[j]) |
            (static_cast<UINT4>(input[j+1]) << 8) |
	    (static_cast<UINT4>(input[j+2]) << 16) |
            (static_cast<UINT4>(input[j+3]) << 24);
}

// Public functions

MD5::MD5()
{
    init();
}

void MD5::reset()
{
    init();
}

void MD5::encodeString(char const* str)
{
    unsigned int len = strlen(str);

    update(QUtil::unsigned_char_pointer(str), len);
    final();
}

void MD5::appendString(char const* input_string)
{
    update(QUtil::unsigned_char_pointer(input_string), strlen(input_string));
}

void MD5::encodeDataIncrementally(char const* data, int len)
{
    update(QUtil::unsigned_char_pointer(data), len);
}

void MD5::encodeFile(char const *filename, int up_to_size)
{
    unsigned char buffer[1024];

    FILE *file = QUtil::safe_fopen(filename, "rb");
    size_t len;
    int so_far = 0;
    int to_try = 1024;
    do
    {
	if ((up_to_size >= 0) && ((so_far + to_try) > up_to_size))
	{
	    to_try = up_to_size - so_far;
	}
	len = fread(buffer, 1, to_try, file);
	if (len > 0)
	{
	    update(buffer, len);
	    so_far += len;
	    if ((up_to_size >= 0) && (so_far >= up_to_size))
	    {
		break;
	    }
	}
    } while (len > 0);
    if (ferror(file))
    {
	// Assume, perhaps incorrectly, that errno was set by the
	// underlying call to read....
	(void) fclose(file);
	QUtil::throw_system_error(
	    std::string("MD5: read error on ") + filename);
    }
    (void) fclose(file);

    final();
}

void MD5::digest(Digest result)
{
    final();
    memcpy(result, digest_val, sizeof(digest_val));
}

void MD5::print()
{
    final();

    unsigned int i;
    for (i = 0; i < 16; ++i)
    {
	printf("%02x", digest_val[i]);
    }
    printf("\n");
}

std::string MD5::unparse()
{
    final();
    return QUtil::hex_encode(
        std::string(reinterpret_cast<char*>(digest_val), 16));
}

std::string
MD5::getDataChecksum(char const* buf, int len)
{
    MD5 m;
    m.encodeDataIncrementally(buf, len);
    return m.unparse();
}

std::string
MD5::getFileChecksum(char const* filename, int up_to_size)
{
    MD5 m;
    m.encodeFile(filename, up_to_size);
    return m.unparse();
}

bool
MD5::checkDataChecksum(char const* const checksum,
		       char const* buf, int len)
{
    std::string actual_checksum = getDataChecksum(buf, len);
    return (checksum == actual_checksum);
}

bool
MD5::checkFileChecksum(char const* const checksum,
		       char const* filename, int up_to_size)
{
    bool result = false;
    try
    {
	std::string actual_checksum = getFileChecksum(filename, up_to_size);
	result = (checksum == actual_checksum);
    }
    catch (std::runtime_error)
    {
	// Ignore -- return false
    }
    return result;
}

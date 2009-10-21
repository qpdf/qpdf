#ifndef __MD5_HH__
#define __MD5_HH__

#include <string>
#include <qpdf/DLL.h>
#include <qpdf/qpdf-config.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

class MD5
{
  public:
    typedef unsigned char Digest[16];

    DLL_EXPORT
    MD5();
    DLL_EXPORT
    void reset();

    // encodes string and finalizes
    DLL_EXPORT
    void encodeString(char const* input_string);

    // encodes file and finalizes
    DLL_EXPORT
    void encodeFile(char const* filename, int up_to_size = -1);

    // appends string to current md5 object
    DLL_EXPORT
    void appendString(char const* input_string);

    // appends arbitrary data to current md5 object
    DLL_EXPORT
    void encodeDataIncrementally(char const* input_data, int len);

    // computes a raw digest
    DLL_EXPORT
    void digest(Digest);

    // prints the digest to stdout terminated with \r\n (primarily for
    // testing)
    DLL_EXPORT
    void print();

    // returns the digest as a hexadecimal string
    DLL_EXPORT
    std::string unparse();

    // Convenience functions
    DLL_EXPORT
    static std::string getDataChecksum(char const* buf, int len);
    DLL_EXPORT
    static std::string getFileChecksum(char const* filename,
				       int up_to_size = -1);
    DLL_EXPORT
    static bool checkDataChecksum(char const* const checksum,
				  char const* buf, int len);
    DLL_EXPORT
    static bool checkFileChecksum(char const* const checksum,
				  char const* filename, int up_to_size = -1);

  private:
    // POINTER defines a generic pointer type
    typedef void *POINTER;

    // UINT2 defines a two byte word
    typedef uint16_t UINT2;

    // UINT4 defines a four byte word
    typedef uint32_t UINT4;

    void init();
    void update(unsigned char *, unsigned int);
    void final();

    static void transform(UINT4 [4], unsigned char [64]);
    static void encode(unsigned char *, UINT4 *, unsigned int);
    static void decode(UINT4 *, unsigned char *, unsigned int);

    UINT4 state[4];		// state (ABCD)
    UINT4 count[2];		// number of bits, modulo 2^64 (lsb first)
    unsigned char buffer[64];	// input buffer

    bool finalized;
    Digest digest_val;
};

#endif // __MD5_HH__

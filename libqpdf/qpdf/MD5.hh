#ifndef __MD5_HH__
#define __MD5_HH__

#include <qpdf/DLL.h>
#include <qpdf/qpdf-config.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <string>

class MD5
{
  public:
    typedef unsigned char Digest[16];

    QPDF_DLL
    MD5();
    QPDF_DLL
    void reset();

    // encodes string and finalizes
    QPDF_DLL
    void encodeString(char const* input_string);

    // encodes file and finalizes
    QPDF_DLL
    void encodeFile(char const* filename, int up_to_size = -1);

    // appends string to current md5 object
    QPDF_DLL
    void appendString(char const* input_string);

    // appends arbitrary data to current md5 object
    QPDF_DLL
    void encodeDataIncrementally(char const* input_data, int len);

    // computes a raw digest
    QPDF_DLL
    void digest(Digest);

    // prints the digest to stdout terminated with \r\n (primarily for
    // testing)
    QPDF_DLL
    void print();

    // returns the digest as a hexadecimal string
    QPDF_DLL
    std::string unparse();

    // Convenience functions
    QPDF_DLL
    static std::string getDataChecksum(char const* buf, int len);
    QPDF_DLL
    static std::string getFileChecksum(char const* filename,
				       int up_to_size = -1);
    QPDF_DLL
    static bool checkDataChecksum(char const* const checksum,
				  char const* buf, int len);
    QPDF_DLL
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

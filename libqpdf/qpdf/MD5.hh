#ifndef MD5_HH
#define MD5_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/Types.h>
#include <memory>
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

    // encodes file and finalizes; offset < 0 reads whole file
    QPDF_DLL
    void encodeFile(char const* filename, qpdf_offset_t up_to_offset = -1);

    // appends string to current md5 object
    QPDF_DLL
    void appendString(char const* input_string);

    // appends arbitrary data to current md5 object
    QPDF_DLL
    void encodeDataIncrementally(char const* input_data, size_t len);

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
    static std::string getDataChecksum(char const* buf, size_t len);
    QPDF_DLL
    static std::string
    getFileChecksum(char const* filename, qpdf_offset_t up_to_offset = -1);
    QPDF_DLL
    static bool
    checkDataChecksum(char const* const checksum, char const* buf, size_t len);
    QPDF_DLL
    static bool checkFileChecksum(
        char const* const checksum,
        char const* filename,
        qpdf_offset_t up_to_offset = -1);

  private:
    void init();
    void finalize();

    std::shared_ptr<QPDFCryptoImpl> crypto;
};

#endif // MD5_HH

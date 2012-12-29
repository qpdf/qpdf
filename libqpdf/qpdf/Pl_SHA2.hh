#ifndef __PL_SHA2_HH__
#define __PL_SHA2_HH__

// Bits must be a supported number of bits, currently only 256, 384,
// or 512.  Passing 0 as bits leaves the pipeline uncommitted, in
// which case resetBits must be called before the pipeline is used.
// If a next is provided, this pipeline sends its output to its
// successor unmodified.  After calling finish, the SHA2 checksum of
// the data that passed through the pipeline is available.

// This pipeline is reusable; i.e., it is safe to call write() after
// calling finish().  The first call to write() after a call to
// finish() initializes a new SHA2 object.  resetBits may also be
// called between finish and the next call to write.

#include <qpdf/Pipeline.hh>
#include <sph/sph_sha2.h>

class Pl_SHA2: public Pipeline
{
  public:
    QPDF_DLL
    Pl_SHA2(int bits = 0, Pipeline* next = 0);
    QPDF_DLL
    virtual ~Pl_SHA2();
    QPDF_DLL
    virtual void write(unsigned char*, size_t);
    QPDF_DLL
    virtual void finish();
    QPDF_DLL
    void resetBits(int bits);
    QPDF_DLL
    std::string getHexDigest();
    QPDF_DLL
    std::string getRawDigest();

  private:
    void badBits();

    bool in_progress;
    int bits;
    sph_sha256_context ctx256;
    sph_sha384_context ctx384;
    sph_sha512_context ctx512;
    unsigned char sha256sum[32];
    unsigned char sha384sum[48];
    unsigned char sha512sum[64];
};

#endif // __PL_SHA2_HH__

#ifndef PL_SHA2_HH
#define PL_SHA2_HH

#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <memory>

// Bits must be a supported number of bits, currently only 256, 384, or 512.  Passing 0 as bits
// leaves the pipeline uncommitted, in which case resetBits must be called before the pipeline is
// used. If a next is provided, this pipeline sends its output to its successor unmodified.  After
// calling finish, the SHA2 checksum of the data that passed through the pipeline is available.
//
// This pipeline is reusable; i.e., it is safe to call write() after calling finish().  The first
// call to write() after a call to finish() initializes a new SHA2 object.  resetBits may also be
// called between finish and the next call to write.
class Pl_SHA2 final: public Pipeline
{
  public:
    Pl_SHA2(int bits = 0, Pipeline* next = nullptr);
    ~Pl_SHA2() final = default;
    void write(unsigned char const*, size_t) final;
    void finish() final;
    void resetBits(int bits);
    std::string getHexDigest();
    std::string getRawDigest();

  private:
    bool in_progress;
    std::shared_ptr<QPDFCryptoImpl> crypto;
};

#endif // PL_SHA2_HH

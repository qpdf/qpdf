#ifndef PL_MD5_HH
#define PL_MD5_HH

#include <qpdf/MD5.hh>
#include <qpdf/Pipeline.hh>

// This pipeline sends its output to its successor unmodified.  After calling finish, the MD5
// checksum of the data that passed through the pipeline is available.

// This pipeline is reusable; i.e., it is safe to call write() after calling finish().  The first
// call to write() after a call to finish() initializes a new MD5 object.
class Pl_MD5 final: public Pipeline
{
  public:
    Pl_MD5(char const* identifier, Pipeline* next);
    ~Pl_MD5() final = default;
    void write(unsigned char const*, size_t) final;
    void finish() final;
    std::string getHexDigest();
    // Enable/disable. Disabling the pipeline causes it to become a pass-through. This makes it
    // possible to stick an MD5 pipeline in a pipeline when it may or may not be required. Disabling
    // it avoids incurring the runtime overhead of doing needless digest computation.
    void enable(bool enabled);
    // If persistAcrossFinish is called, calls to finish do not finalize the underlying md5 object.
    // In this case, the object is not finalized until getHexDigest() is called.
    void persistAcrossFinish(bool);

  private:
    bool in_progress{false};
    MD5 md5;
    bool enabled{true};
    bool persist_across_finish{false};
};

#endif // PL_MD5_HH

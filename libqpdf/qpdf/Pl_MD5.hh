#ifndef __PL_MD5_HH__
#define __PL_MD5_HH__

// This pipeline sends its output to its successor unmodified.  After
// calling finish, the MD5 checksum of the data that passed through
// the pipeline is available.

// This pipeline is reusable; i.e., it is safe to call write() after
// calling finish().  The first call to write() after a call to
// finish() initializes a new MD5 object.

#include <qpdf/Pipeline.hh>
#include <qpdf/MD5.hh>

class Pl_MD5: public Pipeline
{
  public:
    QPDF_DLL
    Pl_MD5(char const* identifier, Pipeline* next);
    QPDF_DLL
    virtual ~Pl_MD5();
    QPDF_DLL
    virtual void write(unsigned char*, size_t);
    QPDF_DLL
    virtual void finish();
    QPDF_DLL
    std::string getHexDigest();
    // Enable/disable. Disabling the pipeline causes it to become a
    // pass-through. This makes it possible to stick an MD5 pipeline
    // in a pipeline when it may or may not be required. Disabling it
    // avoids incurring the runtime overhead of doing needless
    // digest computation.
    QPDF_DLL
    void enable(bool enabled);
    // If persistAcrossFinish is called, calls to finish do not
    // finalize the underlying md5 object. In this case, the object is
    // not finalized until getHexDigest() is called.
    QPDF_DLL
    void persistAcrossFinish(bool);

  private:
    bool in_progress;
    MD5 md5;
    bool enabled;
    bool persist_across_finish;
};

#endif // __PL_MD5_HH__

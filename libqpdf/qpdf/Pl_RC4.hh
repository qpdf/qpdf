#ifndef __PL_RC4_HH__
#define __PL_RC4_HH__

#include <qpdf/Pipeline.hh>

#include <qpdf/RC4.hh>

class Pl_RC4: public Pipeline
{
  public:
    static int const def_bufsize = 65536;

    // key_len of -1 means treat key_data as a null-terminated string
    QPDF_DLL
    Pl_RC4(char const* identifier, Pipeline* next,
	   unsigned char const* key_data, int key_len = -1,
	   size_t out_bufsize = def_bufsize);
    QPDF_DLL
    virtual ~Pl_RC4();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    unsigned char* outbuf;
    size_t out_bufsize;
    RC4 rc4;
};

#endif // __PL_RC4_HH__

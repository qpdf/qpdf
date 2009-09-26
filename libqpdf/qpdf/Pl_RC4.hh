
#ifndef __PL_RC4_HH__
#define __PL_RC4_HH__

#include <qpdf/Pipeline.hh>

#include <qpdf/RC4.hh>

class Pl_RC4: public Pipeline
{
  public:
    class Exception: public Pipeline::Exception
    {
      public:
	DLL_EXPORT
	Exception(std::string const& message) :
	    Pipeline::Exception(message)
	{
	}

	DLL_EXPORT
	virtual ~Exception() throw()
	{
	}
    };

    static int const def_bufsize = 65536;

    // key_len of -1 means treat key_data as a null-terminated string
    DLL_EXPORT
    Pl_RC4(char const* identifier, Pipeline* next,
	   unsigned char const* key_data, int key_len = -1,
	   int out_bufsize = def_bufsize);
    DLL_EXPORT
    virtual ~Pl_RC4();

    DLL_EXPORT
    virtual void write(unsigned char* data, int len);
    DLL_EXPORT
    virtual void finish();

  private:
    unsigned char* outbuf;
    int out_bufsize;
    RC4 rc4;
};

#endif // __PL_RC4_HH__

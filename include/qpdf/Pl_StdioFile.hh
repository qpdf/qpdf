// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

// End-of-line pipeline that simply writes its data to a stdio FILE* object.

#ifndef __PL_STDIOFILE_HH__
#define __PL_STDIOFILE_HH__

#include <qpdf/Pipeline.hh>

#include <stdio.h>

//
// This pipeline is reusable.
//

class Pl_StdioFile: public Pipeline
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
	virtual ~Exception() throw ()
	{
	}
    };

    // f is externally maintained; this class just writes to and
    // flushes it.  It does not close it.
    DLL_EXPORT
    Pl_StdioFile(char const* identifier, FILE* f);
    DLL_EXPORT
    virtual ~Pl_StdioFile();

    DLL_EXPORT
    virtual void write(unsigned char* buf, int len);
    DLL_EXPORT
    virtual void finish();

  private:
    FILE* file;
};

#endif // __PL_STDIOFILE_HH__

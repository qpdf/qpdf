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

class DLL_EXPORT Pl_StdioFile: public Pipeline
{
  public:
    // f is externally maintained; this class just writes to and
    // flushes it.  It does not close it.
    Pl_StdioFile(char const* identifier, FILE* f);
    virtual ~Pl_StdioFile();

    virtual void write(unsigned char* buf, int len);
    virtual void finish();

  private:
    FILE* file;
};

#endif // __PL_STDIOFILE_HH__

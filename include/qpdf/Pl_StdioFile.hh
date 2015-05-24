// Copyright (c) 2005-2015 Jay Berkenbilt
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
    // f is externally maintained; this class just writes to and
    // flushes it.  It does not close it.
    QPDF_DLL
    Pl_StdioFile(char const* identifier, FILE* f);
    QPDF_DLL
    virtual ~Pl_StdioFile();

    QPDF_DLL
    virtual void write(unsigned char* buf, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    FILE* file;
};

#endif // __PL_STDIOFILE_HH__

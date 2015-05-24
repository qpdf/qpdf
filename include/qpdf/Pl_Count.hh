// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __PL_COUNT_HH__
#define __PL_COUNT_HH__

// This pipeline is reusable; i.e., it is safe to call write() after
// calling finish().

#include <qpdf/Types.h>
#include <qpdf/Pipeline.hh>

class Pl_Count: public Pipeline
{
  public:
    QPDF_DLL
    Pl_Count(char const* identifier, Pipeline* next);
    QPDF_DLL
    virtual ~Pl_Count();
    QPDF_DLL
    virtual void write(unsigned char*, size_t);
    QPDF_DLL
    virtual void finish();
    // Returns the number of bytes written
    QPDF_DLL
    qpdf_offset_t getCount() const;
    // Returns the last character written, or '\0' if no characters
    // have been written (in which case getCount() returns 0)
    QPDF_DLL
    unsigned char getLastChar() const;

  private:
    qpdf_offset_t count;
    unsigned char last_char;
};

#endif // __PL_COUNT_HH__

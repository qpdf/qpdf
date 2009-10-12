// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __PL_COUNT_HH__
#define __PL_COUNT_HH__

// This pipeline is reusable; i.e., it is safe to call write() after
// calling finish().

#include <qpdf/Pipeline.hh>

DLL_EXPORT
class Pl_Count: public Pipeline
{
  public:
    Pl_Count(char const* identifier, Pipeline* next);
    virtual ~Pl_Count();
    virtual void write(unsigned char*, int);
    virtual void finish();
    // Returns the number of bytes written
    int getCount() const;
    // Returns the last character written, or '\0' if no characters
    // have been written (in which case getCount() returns 0)
    unsigned char getLastChar() const;

  private:
    int count;
    unsigned char last_char;
};

#endif // __PL_COUNT_HH__

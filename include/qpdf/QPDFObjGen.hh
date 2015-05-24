// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFOBJGEN_HH__
#define __QPDFOBJGEN_HH__

#include <qpdf/DLL.h>

// This class represents an object ID and generation pair.  It is
// suitable to use as a key in a map or set.

class QPDFObjGen
{
  public:
    QPDF_DLL
    QPDFObjGen();
    QPDF_DLL
    QPDFObjGen(int obj, int gen);
    QPDF_DLL
    bool operator<(QPDFObjGen const&) const;
    QPDF_DLL
    bool operator==(QPDFObjGen const&) const;
    QPDF_DLL
    int getObj() const;
    QPDF_DLL
    int getGen() const;

  private:
    int obj;
    int gen;
};

#endif // __QPDFOBJGEN_HH__

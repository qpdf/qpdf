// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFOBJECT_HH__
#define __QPDFOBJECT_HH__

#include <qpdf/DLL.hh>

#include <string>

class QPDFObject
{
  public:
    DLL_EXPORT
    virtual ~QPDFObject() {}
    DLL_EXPORT
    virtual std::string unparse() = 0;
};

#endif // __QPDFOBJECT_HH__

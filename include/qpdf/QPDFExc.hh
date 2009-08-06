// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFEXC_HH__
#define __QPDFEXC_HH__

#include <qpdf/QEXC.hh>

class QPDFExc: public QEXC::General
{
  public:
    DLL_EXPORT
    QPDFExc(std::string const& message);
    DLL_EXPORT
    QPDFExc(std::string const& filename, int offset,
	    std::string const& message);
    DLL_EXPORT
    virtual ~QPDFExc() throw ();
};

#endif // __QPDFEXC_HH__

// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QTC_HH__
#define __QTC_HH__

#include <qpdf/DLL.h>

namespace QTC
{
    QPDF_DLL
    void TC(char const* const scope, char const* const ccase, int n = 0);
};

#endif // __QTC_HH__

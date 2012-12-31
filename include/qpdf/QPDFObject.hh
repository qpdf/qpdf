// Copyright (c) 2005-2013 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFOBJECT_HH__
#define __QPDFOBJECT_HH__

#include <qpdf/DLL.h>

#include <string>

class QPDF;
class QPDFObjectHandle;

class QPDFObject
{
  public:
    virtual ~QPDFObject() {}
    virtual std::string unparse() = 0;

    // Accessor to give specific access to non-public methods
    class ObjAccessor
    {
	friend class QPDF;
	friend class QPDFObjectHandle;
      private:
	static void releaseResolved(QPDFObject* o)
	{
	    if (o)
	    {
		o->releaseResolved();
	    }
	}
    };
    friend class ObjAccessor;

  protected:
    virtual void releaseResolved() {}
};

#endif // __QPDFOBJECT_HH__

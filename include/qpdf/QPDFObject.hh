// Copyright (c) 2005-2015 Jay Berkenbilt
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

    // Objects derived from QPDFObject are accessible through
    // QPDFObjectHandle.  Each object returns a unique type code that
    // has one of the values in the list below.  As new object types
    // are added to qpdf, additional items may be added to the list,
    // so code that switches on these values should take that into
    // consideration.
    enum object_type_e {
        // Object types internal to qpdf
        ot_uninitialized,
        ot_reserved,
        // Object types that can occur in the main document
        ot_null,
        ot_boolean,
        ot_integer,
        ot_real,
        ot_string,
        ot_name,
        ot_array,
        ot_dictionary,
        ot_stream,
        // Additional object types that can occur in content streams
        ot_operator,
        ot_inlineimage,
    };

    virtual ~QPDFObject() {}
    virtual std::string unparse() = 0;

    // Return a unique type code for the object
    virtual object_type_e getTypeCode() const = 0;

    // Return a string literal that describes the type, useful for
    // debugging and testing
    virtual char const* getTypeName() const = 0;

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

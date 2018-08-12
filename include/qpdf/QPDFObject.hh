// Copyright (c) 2005-2018 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef QPDFOBJECT_HH
#define QPDFOBJECT_HH

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>

#include <string>

class QPDF;
class QPDFObjectHandle;

class QPDFObject
{
  public:
    QPDFObject();

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

    virtual void setDescription(QPDF*, std::string const&);
    bool getDescription(QPDF*&, std::string&);
    bool hasDescription();

  protected:
    virtual void releaseResolved() {}

  private:
    QPDFObject(QPDFObject const&);
    QPDFObject& operator=(QPDFObject const&);
    class Members
    {
        friend class QPDFObject;
      public:
        QPDF_DLL
        ~Members();
      private:
        Members();
        QPDF* owning_qpdf;
        std::string object_description;
    };
    PointerHolder<Members> m;
};

#endif // QPDFOBJECT_HH

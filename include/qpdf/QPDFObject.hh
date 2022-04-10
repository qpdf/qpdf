// Copyright (c) 2005-2022 Jay Berkenbilt
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

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/Types.h>

#include <string>

class QPDF;
class QPDFObjectHandle;

class QPDFObject
{
  public:
    QPDFObject();

    // Objects derived from QPDFObject are accessible through
    // QPDFObjectHandle. Each object returns a unique type code that
    // has one of the valid qpdf_object_type_e values. As new object
    // types are added to qpdf, additional items may be added to the
    // list, so code that switches on these values should take that
    // into consideration.

    // Prior to qpdf 10.5, qpdf_object_type_e was
    // QPDFObject::object_type_e but was moved to make it accessible
    // to the C API. The code below is for backward compatibility.
    typedef enum qpdf_object_type_e object_type_e;
    static constexpr object_type_e ot_uninitialized = ::ot_uninitialized;
    static constexpr object_type_e ot_reserved = ::ot_reserved;
    static constexpr object_type_e ot_null = ::ot_null;
    static constexpr object_type_e ot_boolean = ::ot_boolean;
    static constexpr object_type_e ot_integer = ::ot_integer;
    static constexpr object_type_e ot_real = ::ot_real;
    static constexpr object_type_e ot_string = ::ot_string;
    static constexpr object_type_e ot_name = ::ot_name;
    static constexpr object_type_e ot_array = ::ot_array;
    static constexpr object_type_e ot_dictionary = ::ot_dictionary;
    static constexpr object_type_e ot_stream = ::ot_stream;
    static constexpr object_type_e ot_operator = ::ot_operator;
    static constexpr object_type_e ot_inlineimage = ::ot_inlineimage;

    virtual ~QPDFObject()
    {
    }
    virtual std::string unparse() = 0;
    virtual JSON getJSON() = 0;

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
        static void
        releaseResolved(QPDFObject* o)
        {
            if (o) {
                o->releaseResolved();
            }
        }
    };
    friend class ObjAccessor;

    virtual void setDescription(QPDF*, std::string const&);
    bool getDescription(QPDF*&, std::string&);
    bool hasDescription();

    void setParsedOffset(qpdf_offset_t offset);
    qpdf_offset_t getParsedOffset();

  protected:
    virtual void
    releaseResolved()
    {
    }

  private:
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;

    QPDF* owning_qpdf;
    std::string object_description;
    qpdf_offset_t parsed_offset;
};

#endif // QPDFOBJECT_HH

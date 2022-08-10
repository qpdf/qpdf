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
#include <qpdf/QPDFValue.hh>
#include <qpdf/Types.h>

#include <string>

class QPDF;
class QPDFObjectHandle;

class QPDFObject
{
    friend class QPDFValue;

  public:
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
    static constexpr object_type_e ot_unresolved = ::ot_unresolved;

    QPDFObject() = default;
    virtual ~QPDFObject() = default;

    std::shared_ptr<QPDFObject>
    shallowCopy()
    {
        return value->shallowCopy();
    }
    std::string
    unparse()
    {
        return value->unparse();
    }
    JSON
    getJSON(int json_version)
    {
        return value->getJSON(json_version);
    }

    // Return a unique type code for the object
    object_type_e
    getTypeCode() const
    {
        return value->type_code;
    }

    // Return a string literal that describes the type, useful for
    // debugging and testing
    char const*
    getTypeName() const
    {
        return value->type_name;
    }
    void
    setDescription(QPDF* qpdf, std::string const& description)
    {
        return value->setDescription(qpdf, description);
    }
    bool
    getDescription(QPDF*& qpdf, std::string& description)
    {
        return value->getDescription(qpdf, description);
    }
    bool
    hasDescription()
    {
        return value->hasDescription();
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        value->setParsedOffset(offset);
    }
    qpdf_offset_t
    getParsedOffset()
    {
        return value->getParsedOffset();
    }
    void
    assign(std::shared_ptr<QPDFObject> o)
    {
        value = o->value;
    }
    void
    swapWith(std::shared_ptr<QPDFObject> o)
    {
        auto v = value;
        value = o->value;
        o->value = v;
    }
    bool
    isUnresolved()
    {
        return value->type_code == ::ot_unresolved;
    }
    template <typename T>
    T*
    as()
    {
        return dynamic_cast<T*>(value.get());
    }

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

  protected:
    virtual void
    releaseResolved()
    {
        value->releaseResolved();
    }

  private:
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;
    std::shared_ptr<QPDFValue> value;
};

#endif // QPDFOBJECT_HH

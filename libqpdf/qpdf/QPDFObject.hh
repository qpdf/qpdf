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
    qpdf_object_type_e
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
    // Returns nullptr for direct objects
    QPDF*
    getQPDF() const
    {
        return value->qpdf;
    }
    QPDFObjGen
    getObjGen() const
    {
        return value->og;
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
        auto og = value->og;
        value->og = o->value->og;
        o->value->og = og;
    }

    // The following two methods are for use by class QPDF only
    void
    setObjGen(QPDF* qpdf, QPDFObjGen const& og)
    {
        value->qpdf = qpdf;
        value->og = og;
    }
    void
    resetObjGen()
    {
        value->qpdf = nullptr;
        value->og = QPDFObjGen();
    }

    bool
    isUnresolved() const
    {
        return value->type_code == ::ot_unresolved;
    }
    void
    resolve()
    {
        if (isUnresolved()) {
            doResolve();
        }
    }
    void doResolve();

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

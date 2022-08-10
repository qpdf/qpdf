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

#ifndef QPDFVALUE_HH
#define QPDFVALUE_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/Types.h>

#include <string>

class QPDF;
class QPDFObjectHandle;
class QPDFObject;

class QPDFValue
{
    friend class QPDFObject;

  public:
    virtual ~QPDFValue() = default;

    virtual std::shared_ptr<QPDFObject> shallowCopy() = 0;
    virtual std::string unparse() = 0;
    virtual JSON getJSON(int json_version) = 0;
    virtual void
    setDescription(QPDF* qpdf, std::string const& description)
    {
        owning_qpdf = qpdf;
        object_description = description;
    }
    bool
    getDescription(QPDF*& qpdf, std::string& description)
    {
        qpdf = owning_qpdf;
        description = object_description;
        return owning_qpdf != nullptr;
    }
    bool
    hasDescription()
    {
        return owning_qpdf != nullptr;
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        if (parsed_offset < 0) {
            parsed_offset = offset;
        }
    }
    qpdf_offset_t
    getParsedOffset()
    {
        return parsed_offset;
    }

  protected:
    QPDFValue() :
        type_code(::ot_uninitialized),
        type_name("uninitilized")
    {
    }
    QPDFValue(qpdf_object_type_e type_code, char const* type_name) :
        type_code(type_code),
        type_name(type_name)
    {
    }

    virtual void
    releaseResolved()
    {
    }
    static std::shared_ptr<QPDFObject> do_create(QPDFValue*);

  private:
    QPDFValue(QPDFValue const&) = delete;
    QPDFValue& operator=(QPDFValue const&) = delete;
    QPDF* owning_qpdf{nullptr};
    std::string object_description;
    qpdf_offset_t parsed_offset{-1};
    const qpdf_object_type_e type_code;
    char const* type_name;
};

#endif // QPDFVALUE_HH

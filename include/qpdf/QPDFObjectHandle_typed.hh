// Copyright (c) 2005-2024 Jay Berkenbilt
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

#ifndef QPDFOBJECTHANDLE_TYPED_HH
#define QPDFOBJECTHANDLE_TYPED_HH

#include <bitset>

class QPDFObjectHandle::Typed
{
  public:
    operator QPDFObjectHandle() const
    {
        return oh;
    }

    QPDF_DLL
    operator bool() noexcept
    {
        return flags.test(fl_valid);
    }

    QPDF_DLL
    operator bool() const noexcept
    {
        return flags.test(fl_valid);
    }

    QPDF_DLL
    bool
    null() const noexcept
    {
        return flags.test(fl_null);
    }

    QPDF_DLL
    QPDFObjGen
    getObjGen() const
    {
        return oh.getObjGen();
    }

    // ... and all the other methods applicable to all object types.

  protected:
    using Flags = std::bitset<3>;

    enum flags { fl_valid, fl_null, fl_no_type_check };

    Typed() = default;
    Typed(QPDFObjectHandle const& oh, Flags flags) :
        oh(oh),
        flags(flags)
    {
    }

    Typed(QPDFObjectHandle&& oh, Flags flags) :
        oh(std::move(oh)),
        flags(flags)
    {
    }

    Typed(Typed const&) = default;
    Typed(Typed&&) = default;
    Typed& operator=(Typed const&) = default;
    Typed& operator=(Typed&&) = default;
    ~Typed() = default;

    QPDFObjectHandle oh;
    Flags flags{};
};

class QPDFObjectHandle::Integer: public QPDFObjectHandle::Typed
{
    friend class QPDFObjectHandle;

  public:
    QPDF_DLL
    operator long long();

    QPDF_DLL
    operator int();

    QPDF_DLL
    operator unsigned long long();

    QPDF_DLL
    operator unsigned int();

    QPDF_DLL
    long long
    value()
    {
        return *this;
    }

    template <typename T>
    bool
    assign_to(T& var)
    {
        if (flags.test(0)) {
            var = *this;
            return true;
        }
        return false;
    }

  private:
    Integer() = default;
    Integer(QPDFObjectHandle const& oh, Flags flags) :
        QPDFObjectHandle::Typed(oh, flags)
    {
    }
    Integer(QPDFObjectHandle&& oh, QPDF_Integer* ptr, Flags flags) :
        QPDFObjectHandle::Typed(std::move(oh), flags)
    {
    }
};

#endif // QPDFOBJECTHANDLE_TYPED_HH

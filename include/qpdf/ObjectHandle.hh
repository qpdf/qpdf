// Copyright (c) 2005-2021 Jay Berkenbilt
// Copyright (c) 2022-2026 Jay Berkenbilt and Manfred Holger
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under
// the License.
//
// Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic
// License. At your option, you may continue to consider qpdf to be licensed under those terms.
// Please see the manual for additional information.

#ifndef OBJECTHANDLE_HH
#define OBJECTHANDLE_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <qpdf/JSON.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjGen.hh>

#include <cstdint>
#include <memory>

class QPDF;
class QPDF_Dictionary;
class QPDFObject;
class QPDFObjectHandle;

namespace qpdf
{
    class Array;
    class BaseDictionary;
    class Dictionary;
    class Integer;
    class Stream;

    enum typed : std::uint8_t { strict = 0, any_flag = 1, optional = 2, any = 3, error = 4 };

    // Basehandle is only used as a base-class for QPDFObjectHandle like classes. Currently the only
    // methods exposed in public API are operators to convert derived objects to QPDFObjectHandle,
    // QPDFObjGen and bool.
    class BaseHandle
    {
        friend class ::QPDF;

      public:
        explicit inline operator bool() const;
        inline operator QPDFObjectHandle() const;
        QPDF_DLL
        operator QPDFObjGen() const;

        // The rest of the header file is for qpdf internal use only.

        // Return true if both object handles refer to the same underlying object.
        bool
        operator==(BaseHandle const& other) const
        {
            return obj == other.obj;
        }

        // For arrays, return the number of items in the array.
        // For null-like objects, return 0.
        // For all other objects, return 1.
        size_t size() const;

        // Return 'true' if size() == 0.
        bool
        empty() const
        {
            return size() == 0;
        }

        QPDFObjectHandle operator[](size_t n) const;
        QPDFObjectHandle operator[](int n) const;

        QPDFObjectHandle& at(std::string const& key) const;
        bool contains(std::string const& key) const;
        size_t erase(std::string const& key);
        QPDFObjectHandle& find(std::string const& key) const;
        bool replace(std::string const& key, QPDFObjectHandle value);
        QPDFObjectHandle const& operator[](std::string const& key) const;

        std::shared_ptr<QPDFObject> copy(bool shallow = false) const;
        // Recursively remove association with any QPDF object. This method may only be called
        // during final destruction.
        void disconnect(bool only_direct = true);
        inline QPDFObjGen id_gen() const;
        inline bool indirect() const;
        inline bool null() const;
        inline qpdf_offset_t offset() const;
        inline QPDF* qpdf() const;
        inline qpdf_object_type_e raw_type_code() const;
        inline qpdf_object_type_e resolved_type_code() const;
        inline qpdf_object_type_e type_code() const;
        std::string unparse() const;
        void write_json(int json_version, JSON::Writer& p) const;
        static void warn(QPDF*, QPDFExc&&);
        void warn(QPDFExc&&) const;
        void warn(std::string const& warning) const;

        inline std::shared_ptr<QPDFObject> const& obj_sp() const;
        inline QPDFObjectHandle oh() const;

      protected:
        BaseHandle() = default;
        BaseHandle(std::shared_ptr<QPDFObject> const& obj) :
            obj(obj) {};
        BaseHandle(std::shared_ptr<QPDFObject>&& obj) :
            obj(std::move(obj)) {};
        BaseHandle(BaseHandle const&) = default;
        BaseHandle& operator=(BaseHandle const&) = default;
        BaseHandle(BaseHandle&&) = default;
        BaseHandle& operator=(BaseHandle&&) = default;

        inline BaseHandle(QPDFObjectHandle const& oh);
        inline BaseHandle(QPDFObjectHandle&& oh);

        ~BaseHandle() = default;

        template <typename T>
        T* as() const;

        inline void assign(qpdf_object_type_e required, BaseHandle const& other);
        inline void assign(qpdf_object_type_e required, BaseHandle&& other);

        inline void nullify();

        std::string description() const;

        inline QPDFObjectHandle const& get(std::string const& key) const;

        void no_ci_warn_if(bool condition, std::string const& warning) const;
        void no_ci_stop_if(bool condition, std::string const& warning) const;
        void no_ci_stop_damaged_if(bool condition, std::string const& warning) const;
        std::invalid_argument invalid_error(std::string const& method) const;
        std::runtime_error type_error(char const* expected_type) const;
        QPDFExc type_error(char const* expected_type, std::string const& message) const;
        char const* type_name() const;

        std::shared_ptr<QPDFObject> obj;
    };

} // namespace qpdf

#endif // QPDFOBJECTHANDLE_HH

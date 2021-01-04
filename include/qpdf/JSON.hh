// Copyright (c) 2005-2021 Jay Berkenbilt
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

#ifndef JSON_HH
#define JSON_HH

// This is a simple JSON serializer, primarily designed for
// serializing QPDF Objects as JSON. JSON objects contain their data
// as smart pointers. One JSON object is added to another, this
// pointer is copied. This means you can create temporary JSON objects
// on the stack, add them to other objects, and let them go out of
// scope safely. It also means that if the json JSON object is added
// in more than one place, all copies share underlying data.

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>
#include <string>
#include <map>
#include <vector>
#include <list>

class JSON
{
  public:
    QPDF_DLL
    std::string unparse() const;

    // The JSON spec calls dictionaries "objects", but that creates
    // too much confusion when referring to instances of the JSON
    // class.
    QPDF_DLL
    static JSON makeDictionary();
    // addDictionaryMember returns the newly added item.
    QPDF_DLL
    JSON addDictionaryMember(std::string const& key, JSON const&);
    QPDF_DLL
    static JSON makeArray();
    // addArrayElement returns the newly added item.
    QPDF_DLL
    JSON addArrayElement(JSON const&);
    QPDF_DLL
    static JSON makeString(std::string const& utf8);
    QPDF_DLL
    static JSON makeInt(long long int value);
    QPDF_DLL
    static JSON makeReal(double value);
    QPDF_DLL
    static JSON makeNumber(std::string const& encoded);
    QPDF_DLL
    static JSON makeBool(bool value);
    QPDF_DLL
    static JSON makeNull();

    // Check this JSON object against a "schema". This is not a schema
    // according to any standard. It's just a template of what the
    // JSON is supposed to contain. The checking does the following:
    //
    //   * The schema is a nested structure containing dictionaries,
    //     single-element arrays, and strings only.
    //   * Recursively walk the schema
    //   * If the current value is a dictionary, this object must have
    //     a dictionary in the same place with the same keys
    //   * If the current value is an array, this object must have an
    //     array in the same place. The schema's array must contain a
    //     single element, which is used as a schema to validate each
    //     element of this object's corresponding array.
    //   * Otherwise, the value is ignored.
    //
    // QPDF's JSON output conforms to certain strict compatibility
    // rules as discussed in the manual. The idea is that a JSON
    // structure created manually in qpdf.cc doubles as both JSON help
    // information and a schema for validating the JSON that qpdf
    // generates. Any discrepancies are a bug in qpdf.
    QPDF_DLL
    bool checkSchema(JSON schema, std::list<std::string>& errors);

  private:
    static std::string encode_string(std::string const& utf8);

    struct JSON_value
    {
        virtual ~JSON_value();
        virtual std::string unparse(size_t depth) const = 0;
    };
    struct JSON_dictionary: public JSON_value
    {
        virtual ~JSON_dictionary();
        virtual std::string unparse(size_t depth) const;
        std::map<std::string, PointerHolder<JSON_value> > members;
    };
    struct JSON_array: public JSON_value
    {
        virtual ~JSON_array();
        virtual std::string unparse(size_t depth) const;
        std::vector<PointerHolder<JSON_value> > elements;
    };
    struct JSON_string: public JSON_value
    {
        JSON_string(std::string const& utf8);
        virtual ~JSON_string();
        virtual std::string unparse(size_t depth) const;
        std::string encoded;
    };
    struct JSON_number: public JSON_value
    {
        JSON_number(long long val);
        JSON_number(double val);
        JSON_number(std::string const& val);
        virtual ~JSON_number();
        virtual std::string unparse(size_t depth) const;
        std::string encoded;
    };
    struct JSON_bool: public JSON_value
    {
        JSON_bool(bool val);
        virtual ~JSON_bool();
        virtual std::string unparse(size_t depth) const;
        bool value;
    };
    struct JSON_null: public JSON_value
    {
        virtual ~JSON_null();
        virtual std::string unparse(size_t depth) const;
    };

    JSON(PointerHolder<JSON_value>);

    static bool
    checkSchemaInternal(JSON_value* this_v, JSON_value* sch_v,
                        std::list<std::string>& errors,
                        std::string prefix);

    class Members
    {
        friend class JSON;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members(PointerHolder<JSON_value>);
        Members(Members const&);

        PointerHolder<JSON_value> value;
    };

    PointerHolder<Members> m;
};


#endif // JSON_HH

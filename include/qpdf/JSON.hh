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

#ifndef JSON_HH
#define JSON_HH

// This is a simple JSON serializer and parser, primarily designed for
// serializing QPDF Objects as JSON. While it may work as a
// general-purpose JSON parser/serializer, there are better options.
// JSON objects contain their data as smart pointers. When one JSON object
// is added to another, this pointer is copied. This means you can
// create temporary JSON objects on the stack, add them to other
// objects, and let them go out of scope safely. It also means that if
// a JSON object is added in more than one place, all copies
// share the underlying data. This makes them similar in structure and
// behavior to QPDFObjectHandle and may feel natural within the QPDF
// codebase, but it is also a good reason not to use this as a
// general-purpose JSON package.

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

    QPDF_DLL
    bool isArray() const;

    QPDF_DLL
    bool isDictionary() const;

    // Accessors. Accessor behavior:
    //
    // - If argument is wrong type, including null, return false
    // - If argument is right type, return true and initialize the value
    QPDF_DLL
    bool getString(std::string& utf8) const;
    QPDF_DLL
    bool getNumber(std::string& value) const;
    QPDF_DLL
    bool getBool(bool& value) const;
    QPDF_DLL
    bool isNull() const;
    QPDF_DLL
    bool forEachDictItem(
        std::function<void(std::string const& key, JSON value)> fn) const;
    QPDF_DLL
    bool forEachArrayItem(std::function<void(JSON value)> fn) const;

    // Check this JSON object against a "schema". This is not a schema
    // according to any standard. It's just a template of what the
    // JSON is supposed to contain. The checking does the following:
    //
    //   * The schema is a nested structure containing dictionaries,
    //     single-element arrays, and strings only.
    //   * Recursively walk the schema.
    //   * If the current value is a dictionary, this object must have
    //     a dictionary in the same place with the same keys. If flags
    //     contains f_optional, a key in the schema does not have to
    //     be present in the object. Otherwise, all keys have to be
    //     present. Any key in the object must be present in the
    //     schema.
    //   * If the current value is an array, this object must have an
    //     array in the same place. The schema's array must contain a
    //     single element, which is used as a schema to validate each
    //     element of this object's corresponding array.
    //   * Otherwise, the value must be a string whose value is a
    //     description of the object's corresponding value, which may
    //     have any type.
    //
    // QPDF's JSON output conforms to certain strict compatibility
    // rules as discussed in the manual. The idea is that a JSON
    // structure created manually in qpdf.cc doubles as both JSON help
    // information and a schema for validating the JSON that qpdf
    // generates. Any discrepancies are a bug in qpdf.
    //
    // Flags is a bitwise or of values from check_flags_e.
    enum check_flags_e {
        f_none = 0,
        f_optional = 1 << 0,
    };
    QPDF_DLL
    bool checkSchema(
        JSON schema, unsigned long flags, std::list<std::string>& errors);

    // Same as passing 0 for flags
    QPDF_DLL
    bool checkSchema(JSON schema, std::list<std::string>& errors);

    // An pointer to a Reactor class can be passed to parse, which
    // will enable the caller to react to incremental events in the
    // construction of the JSON object. This makes it possible to
    // implement SAX-like handling of very large JSON objects.
    class QPDF_DLL_CLASS Reactor
    {
      public:
        QPDF_DLL
        virtual ~Reactor() = default;

        // The start/end methods are called when parsing of a
        // dictionary or array is started or ended. The item methods
        // are called when an item is added to a dictionary or array.
        // See important notes in "Item methods" below.

        // During parsing of a JSON string, the parser is operating on
        // a single object at a time. When a dictionary or array is
        // started, a new context begins, and when that dictionary or
        // array is ended, the previous context is resumed. So, for
        // example, if you have `{"a": [1]}`, you will receive the
        // following method calls
        //
        // dictionaryStart -- current object is the top-level dictionary
        // arrayStart      -- current object is the array
        // arrayItem       -- called with the "1" object
        // containerEnd    -- now current object is the dictionary again
        // dictionaryItem  -- called with "a" and the just-completed array
        // containerEnd    -- current object is undefined
        //
        // If the top-level item in a JSON string is a scalar, the
        // topLevelScalar() method will be called. No argument is
        // passed since the object is the same as what is returned by
        // parse().

        QPDF_DLL
        virtual void dictionaryStart() = 0;
        QPDF_DLL
        virtual void arrayStart() = 0;
        QPDF_DLL
        virtual void containerEnd(JSON const& value) = 0;
        QPDF_DLL
        virtual void topLevelScalar() = 0;

        // Item methods:
        //
        // The return value of the item methods indicate whether the
        // item has been "consumed". If the item method returns true,
        // then the item will not be added to the containing JSON
        // object. This is what allows arbitrarily large JSON objects
        // to be parsed and not have to be kept in memory.
        //
        // NOTE: When a dictionary or an array is added to a
        // container, the dictionaryItem or arrayItem method is called
        // when the child item's start delimiter is encountered, so
        // the JSON object passed in at that time will always be
        // in its initial, empty state.

        QPDF_DLL
        virtual bool
        dictionaryItem(std::string const& key, JSON const& value) = 0;
        QPDF_DLL
        virtual bool arrayItem(JSON const& value) = 0;
    };

    // Create a JSON object from a string. See above for information
    // about how to use the Reactor.
    QPDF_DLL
    static JSON parse(std::string const&, Reactor* reactor = nullptr);

    // parse calls setOffsets to set the inclusive start and
    // non-inclusive end offsets of an object relative to its input
    // string. Otherwise, both values are 0.
    QPDF_DLL
    void setStart(size_t);
    QPDF_DLL
    void setEnd(size_t);
    QPDF_DLL
    size_t getStart() const;
    QPDF_DLL
    size_t getEnd() const;

  private:
    static std::string encode_string(std::string const& utf8);

    struct JSON_value
    {
        virtual ~JSON_value() = default;
        virtual std::string unparse(size_t depth) const = 0;
    };
    struct JSON_dictionary: public JSON_value
    {
        virtual ~JSON_dictionary() = default;
        virtual std::string unparse(size_t depth) const;
        std::map<std::string, std::shared_ptr<JSON_value>> members;
    };
    struct JSON_array: public JSON_value
    {
        virtual ~JSON_array() = default;
        virtual std::string unparse(size_t depth) const;
        std::vector<std::shared_ptr<JSON_value>> elements;
    };
    struct JSON_string: public JSON_value
    {
        JSON_string(std::string const& utf8);
        virtual ~JSON_string() = default;
        virtual std::string unparse(size_t depth) const;
        std::string utf8;
        std::string encoded;
    };
    struct JSON_number: public JSON_value
    {
        JSON_number(long long val);
        JSON_number(double val);
        JSON_number(std::string const& val);
        virtual ~JSON_number() = default;
        virtual std::string unparse(size_t depth) const;
        std::string encoded;
    };
    struct JSON_bool: public JSON_value
    {
        JSON_bool(bool val);
        virtual ~JSON_bool() = default;
        virtual std::string unparse(size_t depth) const;
        bool value;
    };
    struct JSON_null: public JSON_value
    {
        virtual ~JSON_null() = default;
        virtual std::string unparse(size_t depth) const;
    };

    JSON(std::shared_ptr<JSON_value>);

    static bool checkSchemaInternal(
        JSON_value* this_v,
        JSON_value* sch_v,
        unsigned long flags,
        std::list<std::string>& errors,
        std::string prefix);

    class Members
    {
        friend class JSON;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(std::shared_ptr<JSON_value>);
        Members(Members const&) = delete;

        std::shared_ptr<JSON_value> value;
        // start and end are only populated for objects created by parse
        size_t start;
        size_t end;
    };

    std::shared_ptr<Members> m;
};

#endif // JSON_HH

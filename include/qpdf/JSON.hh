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
#include <qpdf/PointerHolder.hh> // unused -- remove in qpdf 12 (see #785)
#include <qpdf/Types.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class Pipeline;
class InputSource;

class JSON
{
  public:
    static int constexpr LATEST = 2;

    QPDF_DLL
    std::string unparse() const;

    // Write the JSON object through a pipeline. The `depth` parameter
    // specifies how deeply nested this is in another JSON structure,
    // which makes it possible to write clean-looking JSON
    // incrementally.
    QPDF_DLL
    void write(Pipeline*, size_t depth = 0) const;

    // Helper methods for writing JSON incrementally.
    //
    // "first" -- Several methods take a `bool& first` parameter. The
    // open methods always set it to true, and the methods to output
    // items always set it to false. This way, the item and close
    // methods can always know whether or not a first item is being
    // written. The intended mode of operation is to start with a new
    // `bool first = true` each time a new container is opened and
    // to pass that `first` through to all the methods that are
    // called to add top-level items to the container as well as to
    // close the container. This lets the JSON object use it to keep
    // track of when it's writing a first object and when it's not. If
    // incrementally writing multiple levels of depth, a new `first`
    // should used for each new container that is opened.
    //
    // "depth" -- Indicate the level of depth. This is used for
    // consistent indentation. When writing incrementally, whenever
    // you call a method to add an item to a container, the value of
    // `depth` should be one more than whatever value is passed to the
    // container open and close methods.

    // Open methods ignore the value of first and set it to false
    QPDF_DLL
    static void writeDictionaryOpen(Pipeline*, bool& first, size_t depth = 0);
    QPDF_DLL
    static void writeArrayOpen(Pipeline*, bool& first, size_t depth = 0);
    // Close methods don't modify first. A true value indicates that
    // we are closing an empty object.
    QPDF_DLL
    static void writeDictionaryClose(Pipeline*, bool first, size_t depth = 0);
    QPDF_DLL
    static void writeArrayClose(Pipeline*, bool first, size_t depth = 0);
    // The item methods use the value of first to determine if this is
    // the first item and always set it to false.
    QPDF_DLL
    static void writeDictionaryItem(
        Pipeline*,
        bool& first,
        std::string const& key,
        JSON const& value,
        size_t depth = 0);
    // Write just the key of a new dictionary item, useful if writing
    // nested structures. Calls writeNext.
    QPDF_DLL
    static void writeDictionaryKey(
        Pipeline* p, bool& first, std::string const& key, size_t depth = 0);
    QPDF_DLL
    static void writeArrayItem(
        Pipeline*, bool& first, JSON const& element, size_t depth = 0);
    // If writing nested structures incrementally, call writeNext
    // before opening a new array or container in the midst of an
    // existing one. The `first` you pass to writeNext should be the
    // one for the parent object. The depth should be the one for the
    // child object. Then start a new `first` for the nested item.
    // Note that writeDictionaryKey and writeArrayItem call writeNext
    // for you, so this is most important when writing subsequent
    // items or container openers to an array.
    QPDF_DLL
    static void writeNext(Pipeline* p, bool& first, size_t depth = 0);

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

    // A blob serializes as a string. The function will be called by
    // JSON with a pipeline and should write binary data to the
    // pipeline but not call finish(). JSON will call finish() at the
    // right time.
    QPDF_DLL
    static JSON makeBlob(std::function<void(Pipeline*)>);

    QPDF_DLL
    bool isArray() const;

    QPDF_DLL
    bool isDictionary() const;

    // If the key is already in the dictionary, return true.
    // Otherwise, mark it as seen and return false. This is primarily
    // intended to be used by the parser to detect duplicate keys when
    // the reactor blocks them from being added to the final
    // dictionary.
    QPDF_DLL
    bool checkDictionaryKeySeen(std::string const& key);

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
    //   * Recursively walk the schema. In the items below, "schema
    //     object" refers to an object in the schema, and "checked
    //     object" refers to the corresponding part of the object
    //     being checked.
    //   * If the schema object is a dictionary, the checked object
    //     must have a dictionary in the same place with the same
    //     keys. If flags contains f_optional, a key in the schema
    //     does not have to be present in the object. Otherwise, all
    //     keys have to be present. Any key in the object must be
    //     present in the schema.
    //   * If the schema object is an array of length 1, the checked
    //     object may either be a single item or an array of items.
    //     The single item or each element of the checked object's
    //     array is validated against the single element of the
    //     schema's array. The rationale behind this logic is that a
    //     single element may appear wherever the schema allows a
    //     variable-length array. This makes it possible to start
    //     allowing an array in the future where a single element was
    //     previously required without breaking backward
    //     compatibility.
    //   * If the schema object is an array of length > 1, the checked
    //     object must be an array of the same length. In this case,
    //     each element of the checked object array is validated
    //     against the corresponding element of the schema array.
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
        // When adding a container to another container, the item
        // method is called with an empty container before the lower
        // container's start method is called. See important notes in
        // "Item methods" below.

        // During parsing of a JSON string, the parser is operating on
        // a single object at a time. When a dictionary or array is
        // started, a new context begins, and when that dictionary or
        // array is ended, the previous context is resumed. So, for
        // example, if you have `{"a": [1]}`, you will receive the
        // following method calls
        //
        // dictionaryStart -- current object is the top-level dictionary
        // dictionaryItem  -- called with "a" and an empty array
        // arrayStart      -- current object is the array
        // arrayItem       -- called with the "1" object
        // containerEnd    -- now current object is the dictionary again
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
        // the JSON object passed in at that time will always be in
        // its initial, empty state. Additionally, the child item's
        // start method is not called until after the parent item's
        // item method is called. This makes it possible to keep track
        // of the current depth level by incrementing level on start
        // methods and decrementing on end methods.

        QPDF_DLL
        virtual bool
        dictionaryItem(std::string const& key, JSON const& value) = 0;
        QPDF_DLL
        virtual bool arrayItem(JSON const& value) = 0;
    };

    // Create a JSON object from a string.
    QPDF_DLL
    static JSON parse(std::string const&);
    // Create a JSON object from an input source. See above for
    // information about how to use the Reactor.
    QPDF_DLL
    static JSON parse(InputSource&, Reactor* reactor = nullptr);

    // parse calls setOffsets to set the inclusive start and
    // non-inclusive end offsets of an object relative to its input
    // string. Otherwise, both values are 0.
    QPDF_DLL
    void setStart(qpdf_offset_t);
    QPDF_DLL
    void setEnd(qpdf_offset_t);
    QPDF_DLL
    qpdf_offset_t getStart() const;
    QPDF_DLL
    qpdf_offset_t getEnd() const;

  private:
    static std::string encode_string(std::string const& utf8);
    static void
    writeClose(Pipeline* p, bool first, size_t depth, char const* delimeter);
    static void writeIndent(Pipeline* p, size_t depth);

    struct JSON_value
    {
        virtual ~JSON_value() = default;
        virtual void write(Pipeline*, size_t depth) const = 0;
    };
    struct JSON_dictionary: public JSON_value
    {
        virtual ~JSON_dictionary() = default;
        virtual void write(Pipeline*, size_t depth) const;
        std::map<std::string, std::shared_ptr<JSON_value>> members;
        std::set<std::string> parsed_keys;
    };
    struct JSON_array: public JSON_value
    {
        virtual ~JSON_array() = default;
        virtual void write(Pipeline*, size_t depth) const;
        std::vector<std::shared_ptr<JSON_value>> elements;
    };
    struct JSON_string: public JSON_value
    {
        JSON_string(std::string const& utf8);
        virtual ~JSON_string() = default;
        virtual void write(Pipeline*, size_t depth) const;
        std::string utf8;
        std::string encoded;
    };
    struct JSON_number: public JSON_value
    {
        JSON_number(long long val);
        JSON_number(double val);
        JSON_number(std::string const& val);
        virtual ~JSON_number() = default;
        virtual void write(Pipeline*, size_t depth) const;
        std::string encoded;
    };
    struct JSON_bool: public JSON_value
    {
        JSON_bool(bool val);
        virtual ~JSON_bool() = default;
        virtual void write(Pipeline*, size_t depth) const;
        bool value;
    };
    struct JSON_null: public JSON_value
    {
        virtual ~JSON_null() = default;
        virtual void write(Pipeline*, size_t depth) const;
    };
    struct JSON_blob: public JSON_value
    {
        JSON_blob(std::function<void(Pipeline*)> fn);
        virtual ~JSON_blob() = default;
        virtual void write(Pipeline*, size_t depth) const;
        std::function<void(Pipeline*)> fn;
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
        qpdf_offset_t start;
        qpdf_offset_t end;
    };

    std::shared_ptr<Members> m;
};

#endif // JSON_HH

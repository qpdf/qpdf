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

#ifndef JSONHANDLER_HH
#define JSONHANDLER_HH

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>
#include <qpdf/JSON.hh>
#include <string>
#include <map>
#include <functional>
#include <stdexcept>
#include <memory>

class JSONHandler
{
  public:
    // Error exception is thrown if there are any errors validating
    // the JSON object.
    class QPDF_DLL_CLASS Error: public std::runtime_error
    {
      public:
        QPDF_DLL
        Error(std::string const&);
    };

    QPDF_DLL
    JSONHandler();

    QPDF_DLL
    ~JSONHandler() = default;

    // Based on the type of handler, expect the object to be of a
    // certain type. JSONHandler::Error is thrown otherwise. Multiple
    // handlers may be registered, which allows the object to be of
    // various types. If an anyHandler is added, no other handler will
    // be called.

    typedef std::function<void(
        std::string const& path, JSON value)> json_handler_t;
    typedef std::function<void(
        std::string const& path)> void_handler_t;
    typedef std::function<void(
        std::string const& path, std::string const& value)> string_handler_t;
    typedef std::function<void(
        std::string const& path, bool value)> bool_handler_t;

    // If an any handler is added, it will be called for any value
    // including null, and no other handler will be called.
    QPDF_DLL
    void addAnyHandler(json_handler_t fn);

    // If any of the remaining handlers are registered, each
    // registered handle will be called.
    QPDF_DLL
    void addNullHandler(void_handler_t fn);
    QPDF_DLL
    void addStringHandler(string_handler_t fn);
    QPDF_DLL
    void addNumberHandler(string_handler_t fn);
    QPDF_DLL
    void addBoolHandler(bool_handler_t fn);

    // Returns a reference to a map: keys are expected object keys,
    // and values are handlers for that object.
    QPDF_DLL
    std::map<std::string, std::shared_ptr<JSONHandler>>& addDictHandlers();

    // Apply the given handler to any key not explicitly in dict
    // handlers.
    QPDF_DLL
    void addFallbackDictHandler(std::shared_ptr<JSONHandler>);

    // Apply the given handler to each element of the array.
    QPDF_DLL
    void addArrayHandler(std::shared_ptr<JSONHandler>);

    // Apply handlers recursively to a JSON object.
    QPDF_DLL
    void handle(std::string const& path, JSON j);

  private:
    JSONHandler(JSONHandler const&) = delete;

    struct Handlers
    {
        Handlers() :
            any_handler(nullptr),
            null_handler(nullptr),
            string_handler(nullptr),
            number_handler(nullptr),
            bool_handler(nullptr)
        {
        }

        json_handler_t any_handler;
        void_handler_t null_handler;
        string_handler_t string_handler;
        string_handler_t number_handler;
        bool_handler_t bool_handler;
        std::map<std::string, std::shared_ptr<JSONHandler>> dict_handlers;
        std::shared_ptr<JSONHandler> fallback_dict_handler;
        std::shared_ptr<JSONHandler> array_handler;
    };

    class Members
    {
        friend class JSONHandler;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members();
        Members(Members const&) = delete;

        Handlers h;
    };
    PointerHolder<Members> m;
};

#endif // JSONHANDLER_HH

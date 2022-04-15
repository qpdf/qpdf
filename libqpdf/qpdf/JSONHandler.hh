#ifndef JSONHANDLER_HH
#define JSONHANDLER_HH

#include <qpdf/JSON.hh>
#include <functional>
#include <map>
#include <memory>
#include <string>

// This class allows a sax-like walk through a JSON object with
// functionality that mostly mirrors QPDFArgParser. It is primarily
// here to facilitate automatic generation of some of the code to help
// keep QPDFJob json consistent with command-line arguments.

class JSONHandler
{
  public:
    // A QPDFUsage exception is thrown if there are any errors
    // validating the JSON object.
    JSONHandler();
    ~JSONHandler() = default;

    // Based on the type of handler, expect the object to be of a
    // certain type. QPDFUsage is thrown otherwise. Multiple handlers
    // may be registered, which allows the object to be of various
    // types. If an anyHandler is added, no other handler will be
    // called. There is no "final" handler -- if the top-level is a
    // dictionary or array, just use its end handler.

    typedef std::function<void(std::string const& path, JSON value)>
        json_handler_t;
    typedef std::function<void(std::string const& path)> void_handler_t;
    typedef std::function<void(
        std::string const& path, std::string const& value)>
        string_handler_t;
    typedef std::function<void(std::string const& path, bool value)>
        bool_handler_t;

    // If an any handler is added, it will be called for any value
    // including null, and no other handler will be called.
    void addAnyHandler(json_handler_t fn);

    // If any of the remaining handlers are registered, each
    // registered handle will be called.
    void addNullHandler(void_handler_t fn);
    void addStringHandler(string_handler_t fn);
    void addNumberHandler(string_handler_t fn);
    void addBoolHandler(bool_handler_t fn);

    void addDictHandlers(json_handler_t start_fn, void_handler_t end_fn);
    void
    addDictKeyHandler(std::string const& key, std::shared_ptr<JSONHandler>);
    void addFallbackDictHandler(std::shared_ptr<JSONHandler>);

    void addArrayHandlers(
        json_handler_t start_fn,
        void_handler_t end_fn,
        std::shared_ptr<JSONHandler> item_handlers);

    // Apply handlers recursively to a JSON object.
    void handle(std::string const& path, JSON j);

  private:
    JSONHandler(JSONHandler const&) = delete;

    static void usage(std::string const& msg);

    struct Handlers
    {
        Handlers() :
            any_handler(nullptr),
            null_handler(nullptr),
            string_handler(nullptr),
            number_handler(nullptr),
            bool_handler(nullptr),
            dict_start_handler(nullptr),
            dict_end_handler(nullptr),
            array_start_handler(nullptr),
            array_end_handler(nullptr),
            final_handler(nullptr)
        {
        }

        json_handler_t any_handler;
        void_handler_t null_handler;
        string_handler_t string_handler;
        string_handler_t number_handler;
        bool_handler_t bool_handler;
        json_handler_t dict_start_handler;
        void_handler_t dict_end_handler;
        json_handler_t array_start_handler;
        void_handler_t array_end_handler;
        void_handler_t final_handler;
        std::map<std::string, std::shared_ptr<JSONHandler>> dict_handlers;
        std::shared_ptr<JSONHandler> fallback_dict_handler;
        std::shared_ptr<JSONHandler> array_item_handler;
    };

    class Members
    {
        friend class JSONHandler;

      public:
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;

        Handlers h;
    };
    std::shared_ptr<Members> m;
};

#endif // JSONHANDLER_HH

#ifndef JSONHANDLER_HH
#define JSONHANDLER_HH

#include <qpdf/JSON.hh>
#include <functional>
#include <map>
#include <memory>
#include <string>

// This class allows a sax-like walk through a JSON object with functionality that mostly mirrors
// QPDFArgParser. It is primarily here to facilitate automatic generation of some of the code to
// help keep QPDFJob json consistent with command-line arguments.

class JSONHandler
{
  public:
    // A QPDFUsage exception is thrown if there are any errors validating the JSON object.
    JSONHandler();
    ~JSONHandler();

    // Based on the type of handler, expect the object to be of a certain type. QPDFUsage is thrown
    // otherwise. Multiple handlers may be registered, which allows the object to be of various
    // types. If an anyHandler is added, no other handler will be called. There is no "final"
    // handler -- if the top-level is a dictionary or array, just use its end handler.

    typedef std::function<void(std::string const& path, JSON value)> json_handler_t;
    typedef std::function<void(std::string const& path)> void_handler_t;
    typedef std::function<void(std::string const& path, std::string const& value)> string_handler_t;
    typedef std::function<void(std::string const& path, bool value)> bool_handler_t;

    // If an any handler is added, it will be called for any value including null, and no other
    // handler will be called.
    void addAnyHandler(json_handler_t fn);

    // If any of the remaining handlers are registered, each registered handle will be called.
    void addNullHandler(void_handler_t fn);
    void addStringHandler(string_handler_t fn);
    void addNumberHandler(string_handler_t fn);
    void addBoolHandler(bool_handler_t fn);

    void addDictHandlers(json_handler_t start_fn, void_handler_t end_fn);
    void addDictKeyHandler(std::string const& key, std::shared_ptr<JSONHandler>);
    void addFallbackDictHandler(std::shared_ptr<JSONHandler>);

    void addArrayHandlers(
        json_handler_t start_fn, void_handler_t end_fn, std::shared_ptr<JSONHandler> item_handlers);

    // Apply handlers recursively to a JSON object.
    void handle(std::string const& path, JSON j);

  private:
    JSONHandler(JSONHandler const&) = delete;

    static void usage(std::string const& msg);


    class Members;

    std::unique_ptr<Members> m;
};

#endif // JSONHANDLER_HH

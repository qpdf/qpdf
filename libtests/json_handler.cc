#include <qpdf/assert_test.h>

#include <qpdf/JSONHandler.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QUtil.hh>
#include <iostream>

static void
print_null(std::string const& path)
{
    std::cout << path << ": null" << std::endl;
}

static void
print_string(std::string const& path, std::string const& value)
{
    std::cout << path << ": string: " << value << std::endl;
}

static void
print_number(std::string const& path, std::string const& value)
{
    std::cout << path << ": number: " << value << std::endl;
}

static void
print_bool(std::string const& path, bool value)
{
    std::cout << path << ": bool: " << (value ? "true" : "false") << std::endl;
}

static void
print_json(std::string const& path, JSON value)
{
    std::cout << path << ": json: " << value.unparse() << std::endl;
}

static JSONHandler::void_handler_t
make_print_message(std::string msg)
{
    return [msg](std::string const& path) { std::cout << path << ": json: " << msg << std::endl; };
}

static void
test_scalar()
{
    std::cout << "-- scalar --" << std::endl;
    JSONHandler h;
    h.addStringHandler(print_string);
    JSON j = JSON::parse("\"potato\"");
    h.handle(".", j);
}

static std::shared_ptr<JSONHandler>
make_all_handler()
{
    auto h = std::make_shared<JSONHandler>();
    h->addDictHandlers(print_json, make_print_message("dict end"));
    auto h1 = std::make_shared<JSONHandler>();
    h1->addStringHandler(print_string);
    h->addDictKeyHandler("one", h1);
    auto h2 = std::make_shared<JSONHandler>();
    h2->addNumberHandler(print_number);
    h->addDictKeyHandler("two", h2);
    auto h3 = std::make_shared<JSONHandler>();
    h3->addBoolHandler(print_bool);
    h->addDictKeyHandler("three", h3);
    auto h4 = std::make_shared<JSONHandler>();
    h4->addAnyHandler(print_json);
    h->addDictKeyHandler("four", h4);
    h->addDictKeyHandler("phour", h4); // share h4
    auto h5 = std::make_shared<JSONHandler>();
    // Allow to be either string or bool
    h5->addBoolHandler(print_bool);
    h5->addStringHandler(print_string);
    h5->addNullHandler(print_null);
    auto h5s = std::make_shared<JSONHandler>();
    h->addDictKeyHandler("five", h5s);
    h5s->addArrayHandlers(print_json, make_print_message("array end"), h5);
    h5s->addFallbackHandler(h5);
    auto h6 = std::make_shared<JSONHandler>();
    h6->addDictHandlers(print_json, make_print_message("dict end"));
    auto h6a = std::make_shared<JSONHandler>();
    h6->addDictKeyHandler("a", h6a);
    h6a->addDictHandlers(print_json, make_print_message("dict end"));
    auto h6ab = std::make_shared<JSONHandler>();
    h6a->addDictKeyHandler("b", h6ab);
    auto h6ax = std::make_shared<JSONHandler>();
    h6ax->addAnyHandler(print_json);
    h6a->addFallbackDictHandler(h6ax);
    h6->addDictKeyHandler("b", h6ab); // share
    h6ab->addStringHandler(print_string);
    h->addDictKeyHandler("six", h6);
    return h;
}

static void
test_all()
{
    std::cout << "-- all --" << std::endl;
    auto h = make_all_handler();
    /* cSpell: ignore phour */
    JSON j = JSON::parse(R"({
   "one": "potato",
   "two": 3.14,
   "three": true,
   "four": ["a", 1],
   "five": ["x", false, "y", null, true],
   "phour": null,
   "six": {"a": {"b": "quack", "Q": "baaa"}, "b": "moo"}
})");
    h->handle(".", j);
    std::cerr << "-- fallback --" << std::endl;
    j = JSON::parse(R"({
   "five": "not-array"
})");
    h->handle(".", j);
}

static void
test_errors()
{
    std::cout << "-- errors --" << std::endl;
    auto h = make_all_handler();
    auto t = [h](std::string const& msg, std::function<void()> fn) {
        try {
            fn();
            assert(false);
        } catch (QPDFUsage& e) {
            std::cout << msg << ": " << e.what() << std::endl;
        }
    };

    t("bad type at top", [&h]() { h->handle(".", JSON::makeString("oops")); });
    t("unexpected key", [&h]() {
        JSON j = JSON::parse(R"({"x": "y"})");
        h->handle(".", j);
    });
}

int
main(int argc, char* argv[])
{
    test_scalar();
    test_all();
    test_errors();
    return 0;
}

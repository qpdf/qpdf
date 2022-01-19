#include <qpdf/JSONHandler.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <cassert>

static void print_null(std::string const& path)
{
    std::cout << path << ": null" << std::endl;
}

static void print_string(std::string const& path, std::string const& value)
{
    std::cout << path << ": string: " << value << std::endl;
}

static void print_number(std::string const& path, std::string const& value)
{
    std::cout << path << ": number: " << value << std::endl;
}

static void print_bool(std::string const& path, bool value)
{
    std::cout << path << ": bool: " << (value ? "true" : "false") << std::endl;
}

static void print_json(std::string const& path, JSON value)
{
    std::cout << path << ": json: " << value.unparse() << std::endl;
}

static void test_scalar()
{
    std::cout << "-- scalar --" << std::endl;
    JSONHandler h;
    h.addStringHandler(print_string);
    JSON j = JSON::parse("\"potato\"");
    h.handle(".", j);
}

static std::shared_ptr<JSONHandler> make_all_handler()
{
    auto h = std::make_shared<JSONHandler>();
    auto& m = h->addDictHandlers();
    auto h1 = std::make_shared<JSONHandler>();
    h1->addStringHandler(print_string);
    m["one"] = h1;
    auto h2 = std::make_shared<JSONHandler>();
    h2->addNumberHandler(print_number);
    m["two"] = h2;
    auto h3 = std::make_shared<JSONHandler>();
    h3->addBoolHandler(print_bool);
    m["three"] = h3;
    auto h4 = std::make_shared<JSONHandler>();
    h4->addAnyHandler(print_json);
    m["four"] = h4;
    m["phour"] = h4;           // share h4
    auto h5 = std::make_shared<JSONHandler>();
    // Allow to be either string or bool
    h5->addBoolHandler(print_bool);
    h5->addStringHandler(print_string);
    h5->addNullHandler(print_null);
    auto h5s = std::make_shared<JSONHandler>();
    m["five"] = h5s;
    h5s->addArrayHandler(h5);
    auto h6 = std::make_shared<JSONHandler>();
    auto& m6 = h6->addDictHandlers();
    auto h6a = std::make_shared<JSONHandler>();
    m6["a"] = h6a;
    auto& m6a = h6a->addDictHandlers();
    auto h6ab = std::make_shared<JSONHandler>();
    m6a["b"] = h6ab;
    auto h6ax = std::make_shared<JSONHandler>();
    h6ax->addAnyHandler(print_json);
    h6a->addFallbackDictHandler(h6ax);
    m6["b"] = h6ab;             // share
    h6ab->addStringHandler(print_string);
    m["six"] = h6;
    return h;
}

static void test_all()
{
    std::cout << "-- all --" << std::endl;
    auto h = make_all_handler();
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
}

static void test_errors()
{
    std::cout << "-- errors --" << std::endl;
    auto h = make_all_handler();
    auto t = [h](std::string const& msg, std::function<void()> fn) {
        try
        {
            fn();
            assert(false);
        }
        catch (JSONHandler::Error& e)
        {
            std::cout << msg << ": " << e.what() << std::endl;
        }
    };

    t("bad type at top", [&h](){
        h->handle(".", JSON::makeString("oops"));
    });
    t("unexpected key", [&h](){
        JSON j = JSON::parse(R"({"x": "y"})");
        h->handle(".", j);
    });
}

int main(int argc, char* argv[])
{
    test_scalar();
    test_all();
    test_errors();
    return 0;
}

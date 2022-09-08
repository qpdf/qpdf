#include <qpdf/assert_test.h>

#include <qpdf/Pl_Base64.hh>
#include <qpdf/Pl_Function.hh>
#include <qpdf/Pl_String.hh>
#include <iostream>

namespace
{
    struct Count
    {
        int count{0};
    };
} // namespace

int
f(unsigned char const* data, size_t len, void* udata)
{
    auto c = reinterpret_cast<Count*>(udata);
    ++c->count;
    std::cout << "got " << data << "(" << len << ")" << std::endl;
    if (c->count == 3) {
        return 1;
    }
    return 0;
}

int
g(char const* data, size_t len, void* udata)
{
    auto c = reinterpret_cast<Count*>(udata);
    ++c->count;
    std::cout << "signed got " << data << "(" << len << ")" << std::endl;
    if (c->count == 2) {
        return 2;
    }
    return 0;
}

int
main(int argc, char* argv[])
{
    Pl_Function p1("p1", nullptr, [](unsigned char const* data, size_t len) {
        std::cout << "p1: " << len << ": " << data << std::endl;
    });
    p1.write(reinterpret_cast<unsigned char const*>("potato"), 6);

    std::string s;
    Pl_String ps("string", nullptr, s);
    Pl_Base64 b("base64", &ps, Pl_Base64::a_encode);
    Pl_Function p2("p2", &b, [](unsigned char const* data, size_t len) {
        std::cout << "p2: " << len << ": " << data << std::endl;
    });
    p2.write(reinterpret_cast<unsigned char const*>("salad"), 5);
    p2.finish();
    assert(s == "c2FsYWQ=");

    Count c;
    Pl_Function p3("c-function", nullptr, f, &c);
    p3 << "one";
    p3 << "two";
    try {
        p3 << "three";
        assert(false);
    } catch (std::runtime_error& e) {
        std::cout << "three threw " << e.what() << std::endl;
    }
    p3 << "four";
    p3.finish();
    assert(c.count == 4);

    c.count = 0;
    Pl_Function p4("c-function", nullptr, g, &c);
    p4 << "potato";
    try {
        p4 << "salad";
        assert(false);
    } catch (std::runtime_error& e) {
        std::cout << "salad threw " << e.what() << std::endl;
    }
    p4 << "quack";
    p4.finish();
    assert(c.count == 3);

    return 0;
}

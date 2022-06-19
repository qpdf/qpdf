#include <qpdf/assert_test.h>

#include <qpdf/Pl_Function.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/Pl_Base64.hh>
#include <iostream>

int
main(int argc, char* argv[])
{
    Pl_Function p1(
        "p1", nullptr, [](unsigned char const* data, size_t len) {
            std::cout << "p1: " << len << ": " << data << std::endl;
        });
    p1.write(reinterpret_cast<unsigned char const*>("potato"), 6);

    std::string s;
    Pl_String ps("string", nullptr, s);
    Pl_Base64 b("base64", &ps, Pl_Base64::a_encode);
    Pl_Function p2(
        "p2", &b, [](unsigned char const* data, size_t len) {
            std::cout << "p2: " << len << ": " << data << std::endl;
        });
    p2.write(reinterpret_cast<unsigned char const*>("salad"), 5);
    p2.finish();
    assert(s == "c2FsYWQ=");

    return 0;
}

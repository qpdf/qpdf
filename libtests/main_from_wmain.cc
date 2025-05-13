#include <qpdf/QUtil.hh>
#include <iostream>

#ifndef QPDF_NO_WCHAR_T
void
wmain_test()
{
    // writable args and function args
    auto realmain = [](int argc, char* argv[]) {
        for (int i = 0; i < argc; ++i) {
            std::cout << argv[i] << '\n';
        }
        return 0;
    };
    wchar_t* argv[3];
    // This works because call_main_from_wmain doesn't actually write
    // to the arguments and neither does our function. Otherwise, this
    // cast would be unsafe.
    argv[0] = const_cast<wchar_t*>(L"ascii");
    argv[1] = const_cast<wchar_t*>(L"10 \xf7 2 = 5");
    argv[2] = const_cast<wchar_t*>(L"qwww\xf7\x03c0");
    QUtil::call_main_from_wmain(3, argv, realmain);
}

void
cwmain_test()
{
    // const args and function args
    auto realmain = [](int argc, char const* const argv[]) {
        for (int i = 0; i < argc; ++i) {
            std::cout << "const " << argv[i] << '\n';
        }
        return 0;
    };
    wchar_t const* argv[3] = {
        L"ascii",
        L"10 \xf7 2 = 5",
        L"qwww\xf7\x03c0",
    };
    QUtil::call_main_from_wmain(3, argv, realmain);
}
#endif // QPDF_NO_WCHAR_T

int
main(int argc, char* argv[])
{
#ifndef QPDF_NO_WCHAR_T
    try {
        wmain_test();
        cwmain_test();
    } catch (std::exception& e) {
        std::cout << "unexpected exception: " << e.what() << '\n';
    }
#endif // QPDF_NO_WCHAR_T

    return 0;
}

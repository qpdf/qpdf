#include <qpdf/QUtil.hh>
#include <cstring>
#include <iostream>

int
realmain(int argc, char* argv[])
{
    // In Windows, shell globbing is handled by the runtime, so
    // passing '*' as argument results in wildcard expansion. In
    // non-Windows, globbing is done by the shell, so passing '*'
    // shows up as '*'. In Windows with MSVC, it is necessary to link
    // a certain way for this to work. To test this, we invoke this
    // program with a single quoted argument containing a shell glob
    // expression. In Windows, we expect to see multiple arguments,
    // none of which contain '*'. Otherwise, we expected to see the
    // exact glob string that was passed in. The effectiveness of this
    // test was exercised by manually breaking the link options for
    // msvc and seeing that the test fails under that condition.

    bool found_star = false;
    for (int i = 1; i < argc; ++i) {
        if (strchr(argv[i], '*') != nullptr) {
            found_star = true;
            break;
        }
    }
#ifdef _WIN32
    bool passed = ((!found_star) && (argc > 2));
#else
    bool passed = (found_star && (argc == 2));
#endif
    if (passed) {
        std::cout << "PASSED" << std::endl;
    } else {
        std::cout << "FAILED" << std::endl;
        for (int i = 1; i < argc; ++i) {
            std::cout << argv[i] << std::endl;
        }
    }
    return 0;
}

#ifdef WINDOWS_WMAIN

extern "C" int
wmain(int argc, wchar_t* argv[])
{
    return QUtil::call_main_from_wmain(argc, argv, realmain);
}

#else

int
main(int argc, char* argv[])
{
    return realmain(argc, argv);
}

#endif

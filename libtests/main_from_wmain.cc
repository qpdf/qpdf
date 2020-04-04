#include <qpdf/QUtil.hh>
#include <iostream>

#ifndef QPDF_NO_WCHAR_T
void wmain_test()
{
    auto realmain = [](int argc, char* argv[]) {
                        for (int i = 0; i < argc; ++i) {
                            std::cout << argv[i] << std::endl;
                        } return 0;
                    };
    wchar_t* argv[3];
    argv[0] = const_cast<wchar_t*>(L"ascii");
    argv[1] = const_cast<wchar_t*>(L"10 \xf7 2 = 5");
    argv[2] = const_cast<wchar_t*>(L"qwww\xf7\x03c0");
    QUtil::call_main_from_wmain(3, argv, realmain);
}
#endif // QPDF_NO_WCHAR_T

int main(int argc, char* argv[])
{
#ifndef QPDF_NO_WCHAR_T
    try
    {
	wmain_test();
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
    }
#endif // QPDF_NO_WCHAR_T

    return 0;
}

#include <qpdf/QPDF.hh>
#include <iostream>

int
main()
{
    std::cout << QPDF::QPDFVersion() << std::endl;
    return 0;
}

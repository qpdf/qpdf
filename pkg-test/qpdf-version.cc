#ifndef POINTERHOLDER_TRANSITION
# define POINTERHOLDER_TRANSITION 4
#endif

#include <qpdf/QPDF.hh>
#include <iostream>

int
main()
{
    std::cout << QPDF::QPDFVersion() << std::endl;
    return 0;
}

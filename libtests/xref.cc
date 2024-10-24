#include <qpdf/QPDF.hh>

#include <cstdlib>
#include <iostream>
#include <map>

int
main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: xref INFILE\n";
        std::exit(2);
    }

    try {
        QPDF qpdf;
        qpdf.processFile(argv[1]);
        qpdf.test_xref();
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        std::exit(2);
    }
    std::cout << "xref done\n";
    return 0;
}

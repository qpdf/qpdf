#include <cstdlib>
#include <cstring>
#include <iostream>

#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " filename\n"
              << "Prints the number of pages in filename\n";
    exit(2);
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << whoami << " version 1.3\n";
        exit(0);
    }

    if (argc != 2) {
        usage();
    }
    char const* filename = argv[1];

    try {
        QPDF pdf;
        pdf.processFile(filename);
        QPDFObjectHandle root = pdf.getRoot();
        QPDFObjectHandle pages = root.getKey("/Pages");
        QPDFObjectHandle count = pages.getKey("/Count");
        std::cout << count.getIntValue() << '\n';
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << '\n';
        exit(2);
    }

    return 0;
}

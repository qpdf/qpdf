#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>

static char const* whoami = 0;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile" << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    if ((whoami = strrchr(argv[0], '/')) == NULL) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if (argc != 2) {
        usage();
    }
    char const* infilename = argv[1];
    for (auto const& line: QUtil::read_lines_from_file(infilename)) {
        QPDFObjectHandle str = QPDFObjectHandle::newString(line);
        std::cout << str.getUTF8Value() << std::endl;
    }
    return 0;
}

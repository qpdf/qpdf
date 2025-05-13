#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>
#include <cstring>
#include <iostream>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile" << '\n';
    exit(2);
}

int
main(int argc, char* argv[])
{
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
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
        std::cout << str.getUTF8Value() << '\n';
    }
    return 0;
}

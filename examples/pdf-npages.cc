#include <iostream>
#include <string.h>
#include <stdlib.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " filename" << std::endl
	      << "Prints the number of pages in filename" << std::endl;
    exit(2);
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0))
    {
	std::cout << whoami << " version 1.3" << std::endl;
	exit(0);
    }

    if (argc != 2)
    {
	usage();
    }
    char const* filename = argv[1];

    try
    {
	QPDF pdf;
	pdf.processFile(filename);
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle pages = root.getKey("/Pages");
	QPDFObjectHandle count = pages.getKey("/Count");
	std::cout << count.getIntValue() << std::endl;
    }
    catch (std::exception& e)
    {
	std::cerr << whoami << ": " << e.what() << std::endl;
	exit(2);
    }

    return 0;
}

#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>

#include <iostream>
#include <cstring>

// QXXXQ describe

static char const* whoami = 0;

#if 0 // QXXXQ
static void usage(std::string const& msg)
{
    std::cerr << "Usage: " << whoami << " QXXXQ" << std::endl;
    exit(2);
}
#endif

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    try
    {
        QPDFJob j;
        j.config()
            ->inputFile("/tmp/1.pdf")
            ->outputFile("/tmp/2.pdf")
            ->pages()
            ->pageSpec(".", "1-z")
            ->endPages()
            ->qdf();
        j.run();
    }
    catch (std::exception& e)
    {
        // QXXXQ catch usage, configerror, whatever we end up with separately
        std::cerr << "exception: " << e.what() << std::endl;
        return 2;
    }
    return 0;
}

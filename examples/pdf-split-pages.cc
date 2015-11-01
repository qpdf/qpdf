//
// This is a stand-alone example of splitting a PDF into individual
// pages. It is much faster than using the qpdf command-line tool to
// split into separate files per page.
//

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <string>
#include <iostream>
#include <cstdlib>

static bool static_id = false;

static void process(char const* whoami,
                    char const* infile,
                    std::string outprefix)
{
    QPDF inpdf;
    inpdf.processFile(infile);
    std::vector<QPDFObjectHandle> const& pages = inpdf.getAllPages();
    int pageno_len = QUtil::int_to_string(pages.size()).length();
    int pageno = 0;
    for (std::vector<QPDFObjectHandle>::const_iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFObjectHandle page = *iter;
        std::string outfile =
            outprefix + QUtil::int_to_string(++pageno, pageno_len) + ".pdf";
        QPDF outpdf;
        outpdf.emptyPDF();
        outpdf.addPage(page, false);
        QPDFWriter outpdfw(outpdf, outfile.c_str());
	if (static_id)
	{
	    // For the test suite, uncompress streams and use static
	    // IDs.
	    outpdfw.setStaticID(true); // for testing only
	    outpdfw.setStreamDataMode(qpdf_s_uncompress);
	}
        outpdfw.write();
    }
}

int main(int argc, char* argv[])
{
    char* whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }
    // For test suite
    if ((argc > 1) && (strcmp(argv[1], " --static-id") == 0))
    {
        static_id = true;
        --argc;
        ++argv;
    }

    if (argc != 3)
    {
        std::cerr << "Usage: " << whoami << " infile outprefix" << std::endl;
    }
    try
    {
        process(whoami, argv[1], argv[2]);
    }
    catch (std::exception e)
    {
        std::cerr << whoami << ": exception: " << e.what() << std::endl;
        return 2;
    }
    return 0;
}

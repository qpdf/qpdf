//
// This is a stand-alone example of splitting a PDF into individual
// pages. It does essentially the same thing that qpdf --split-pages
// does.
//

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

#include <iostream>
#include <stdlib.h>
#include <string>

static char const* whoami = 0;
static bool static_id = false;

static void process(char const* whoami,
                    char const* infile,
                    std::string outprefix)
{
    QPDF inpdf;
    inpdf.processFile(infile);
    std::vector<QPDFPageObjectHelper> pages =
        QPDFPageDocumentHelper(inpdf).getAllPages();
    int pageno_len = QUtil::int_to_string(pages.size()).length();
    int pageno = 0;
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFPageObjectHelper& page(*iter);
        std::string outfile =
            outprefix + QUtil::int_to_string(++pageno, pageno_len) + ".pdf";
        QPDF outpdf;
        outpdf.emptyPDF();
        QPDFPageDocumentHelper(outpdf).addPage(page, false);
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

void usage()
{
    std::cerr << "Usage: " << whoami << " infile outprefix" << std::endl;
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
    // For test suite
    if ((argc > 1) && (strcmp(argv[1], " --static-id") == 0))
    {
        static_id = true;
        --argc;
        ++argv;
    }

    if (argc != 3)
    {
        usage();
    }
    try
    {
        process(whoami, argv[1], argv[2]);
    }
    catch (std::exception const& e)
    {
        std::cerr << whoami << ": exception: " << e.what() << std::endl;
        return 2;
    }
    return 0;
}

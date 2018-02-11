//
// This example illustrates the use of QPDFObjectHandle::TokenFilter
// with filterPageContents. See also pdf-filter-tokens.cc for an
// example that uses QPDFObjectHandle::TokenFilter with
// addContentTokenFilter.
//

#include <iostream>
#include <string.h>
#include <stdlib.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/Pl_StdioFile.hh>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " infile" << std::endl
	      << "Applies token filters to infile"
              << std::endl;
    exit(2);
}

class StringCounter: public QPDFObjectHandle::TokenFilter
{
  public:
    StringCounter() :
        count(0)
    {
    }
    virtual ~StringCounter()
    {
    }
    virtual void handleToken(QPDFTokenizer::Token const&);
    virtual void handleEOF();
    int getCount() const;

  private:
    int count;
};

void
StringCounter::handleToken(QPDFTokenizer::Token const& token)
{
    // Count string tokens
    if (token.getType() == QPDFTokenizer::tt_string)
    {
        ++this->count;
    }
    // Preserve input verbatim by passing each token to any specified
    // downstream filter.
    writeToken(token);
}

void
StringCounter::handleEOF()
{
    // Write a comment at the end of the stream just to show how we
    // can enhance the output if we want.
    write("\n% strings found: ");
    write(QUtil::int_to_string(this->count));
    // If you override handleEOF, you must always remember to call finish().
    finish();
}

int
StringCounter::getCount() const
{
    return this->count;
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if (argc != 2)
    {
	usage();
    }
    char const* infilename = argv[1];

    try
    {
	QPDF pdf;
	pdf.processFile(infilename);
        std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
        int pageno = 0;
        for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
             iter != pages.end(); ++iter)
        {
            QPDFObjectHandle page = *iter;
            ++pageno;
            // Pass the contents of a page through our string counter.
            // If it's an even page, capture the output. This
            // illustrates that you may capture any output generated
            // by the filter, or you may ignore it.
            StringCounter counter;
            if (pageno % 2)
            {
                // Ignore output for odd pages.
                page.filterPageContents(&counter);
            }
            else
            {
                // Write output to stdout for even pages.
                Pl_StdioFile out("stdout", stdout);
                std::cout << "% Contents of page " << pageno << std::endl;
                page.filterPageContents(&counter, &out);
                std::cout << "\n% end " << pageno << std::endl;
            }
            std::cout << "Page " << pageno
                      << ": strings = " << counter.getCount() << std::endl;
        }
    }
    catch (std::exception& e)
    {
	std::cerr << whoami << ": " << e.what() << std::endl;
	exit(2);
    }

    return 0;
}

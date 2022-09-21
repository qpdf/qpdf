//
// This example illustrates the use of QPDFObjectHandle::TokenFilter
// with filterContents. See also pdf-filter-tokens.cc for an example
// that uses QPDFObjectHandle::TokenFilter with addContentTokenFilter.
//

#include <iostream>
#include <stdlib.h>
#include <string.h>

#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QUtil.hh>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile" << std::endl
              << "Applies token filters to infile" << std::endl;
    exit(2);
}

class StringCounter: public QPDFObjectHandle::TokenFilter
{
  public:
    StringCounter() :
        count(0)
    {
    }
    virtual ~StringCounter() = default;
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
    if (token.getType() == QPDFTokenizer::tt_string) {
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
    write(std::to_string(this->count));
}

int
StringCounter::getCount() const
{
    return this->count;
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 2) {
        usage();
    }
    char const* infilename = argv[1];

    try {
        QPDF pdf;
        pdf.processFile(infilename);
        int pageno = 0;
        for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
            ++pageno;
            // Pass the contents of a page through our string counter.
            // If it's an even page, capture the output. This
            // illustrates that you may capture any output generated
            // by the filter, or you may ignore it.
            StringCounter counter;
            if (pageno % 2) {
                // Ignore output for odd pages.
                page.filterContents(&counter);
            } else {
                // Write output to stdout for even pages.
                Pl_StdioFile out("stdout", stdout);
                std::cout << "% Contents of page " << pageno << std::endl;
                page.filterContents(&counter, &out);
                std::cout << "\n% end " << pageno << std::endl;
            }
            std::cout << "Page " << pageno
                      << ": strings = " << counter.getCount() << std::endl;
        }
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

#include <iostream>
#include <string.h>
#include <stdlib.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QIntC.hh>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " filename page-number" << std::endl
              << "Prints a dump of the objects in the content streams"
              << " of the given page." << std::endl
              << "Pages are numbered from 1." << std::endl;
    exit(2);
}

class ParserCallbacks: public QPDFObjectHandle::ParserCallbacks
{
  public:
    virtual ~ParserCallbacks()
    {
    }

    virtual void contentSize(size_t);
    virtual void handleObject(QPDFObjectHandle, size_t offset, size_t length);
    virtual void handleEOF();
};

void
ParserCallbacks::contentSize(size_t size)
{
    std::cout << "content size: " << size << std::endl;
}

void
ParserCallbacks::handleObject(QPDFObjectHandle obj,
                              size_t offset, size_t length)
{
    std::cout << obj.getTypeName() << ", offset=" << offset
              << ", length=" << length << ": ";
    if (obj.isInlineImage())
    {
        std::cout << QUtil::hex_encode(obj.getInlineImageValue()) << std::endl;
    }
    else
    {
        std::cout << obj.unparse() << std::endl;
    }
}

void
ParserCallbacks::handleEOF()
{
    std::cout << "-EOF-" << std::endl;
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 3)
    {
        usage();
    }
    char const* filename = argv[1];
    int pageno = QUtil::string_to_int(argv[2]);

    try
    {
        QPDF pdf;
        pdf.processFile(filename);
        std::vector<QPDFPageObjectHelper> pages =
            QPDFPageDocumentHelper(pdf).getAllPages();
        if ((pageno < 1) || (QIntC::to_size(pageno) > pages.size()))
        {
            usage();
        }

        QPDFPageObjectHelper& page = pages.at(QIntC::to_size(pageno-1));
        ParserCallbacks cb;
        page.parseContents(&cb);
    }
    catch (std::exception& e)
    {
        std::cerr << whoami << ": " << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

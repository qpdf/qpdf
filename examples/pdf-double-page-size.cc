#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile.pdf outfile.pdf [in-password]"
              << std::endl
              << "Double size of all pages in infile.pdf;"
              << " write output to outfile.pdf" << std::endl;
    exit(2);
}

// If there is a box of name box_name, replace it with a new box whose
// elements are double the values of the original box.
static void
doubleBoxSize(QPDFPageObjectHelper& page, char const* box_name)
{
    // We need to use getAttribute rather than getKey as some boxes could
    // be inherited.
    auto box = page.getAttribute(box_name, true);
    if (box.isNull()) {
        return;
    }
    if (!box.isRectangle()) {
        throw std::runtime_error(
            std::string("box ") + box_name +
            " is not an array of four elements");
    }
    std::vector<QPDFObjectHandle> doubled;
    for (auto& item: box.aitems()) {
        doubled.push_back(
            QPDFObjectHandle::newReal(item.getNumericValue() * 2.0, 2));
    }
    page.getObjectHandle().replaceKey(
        box_name, QPDFObjectHandle::newArray(doubled));
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For test suite
    bool static_id = false;
    if ((argc > 1) && (strcmp(argv[1], " --static-id") == 0)) {
        static_id = true;
        --argc;
        ++argv;
    }

    if (!((argc == 3) || (argc == 4))) {
        usage();
    }

    char const* infilename = argv[1];
    char const* outfilename = argv[2];
    char const* password = (argc == 4) ? argv[3] : "";

    // Text to prepend to each page's contents
    std::string content = "2 0 0 2 0 0 cm\n";

    try {
        QPDF qpdf;
        qpdf.processFile(infilename, password);

        for (auto& page: QPDFPageDocumentHelper(qpdf).getAllPages()) {
            // Prepend the buffer to the page's contents
            page.addPageContents(qpdf.newStream(content), true);

            // Double the size of each of the content boxes
            doubleBoxSize(page, "/MediaBox");
            doubleBoxSize(page, "/CropBox");
            doubleBoxSize(page, "/BleedBox");
            doubleBoxSize(page, "/TrimBox");
            doubleBoxSize(page, "/ArtBox");
        }

        // Write out a new file
        QPDFWriter w(qpdf, outfilename);
        if (static_id) {
            // For the test suite, uncompress streams and use static IDs.
            w.setStaticID(true); // for testing only
            w.setStreamDataMode(qpdf_s_uncompress);
        }
        w.write();
        std::cout << whoami << ": new file written to " << outfilename
                  << std::endl;
    } catch (std::exception& e) {
        std::cerr << whoami << " processing file " << infilename << ": "
                  << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

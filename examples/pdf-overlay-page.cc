#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>

// This program demonstrates use of form XObjects to overlay a page
// from one file onto all pages of another file. The qpdf program's
// --overlay and --underlay options provide a more general version of
// this capability.

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile pagefile outfile" << std::endl
              << "Stamp page 1 of pagefile on every page of infile,"
              << " writing to outfile" << std::endl;
    exit(2);
}

static void
stamp_page(char const* infile, char const* stampfile, char const* outfile)
{
    QPDF inpdf;
    inpdf.processFile(infile);
    QPDF stamppdf;
    stamppdf.processFile(stampfile);

    // Get first page from other file
    QPDFPageObjectHelper stamp_page_1 =
        QPDFPageDocumentHelper(stamppdf).getAllPages().at(0);
    // Convert page to a form XObject
    QPDFObjectHandle foreign_fo = stamp_page_1.getFormXObjectForPage();
    // Copy form XObject to the input file
    QPDFObjectHandle stamp_fo = inpdf.copyForeignObject(foreign_fo);

    // For each page...
    for (auto& ph: QPDFPageDocumentHelper(inpdf).getAllPages()) {
        // Find a unique resource name for the new form XObject
        QPDFObjectHandle resources = ph.getAttribute("/Resources", true);
        int min_suffix = 1;
        std::string name = resources.getUniqueResourceName("/Fx", min_suffix);

        // Generate content to place the form XObject centered within
        // destination page's trim box.
        QPDFMatrix m;
        std::string content = ph.placeFormXObject(
            stamp_fo, name, ph.getTrimBox().getArrayAsRectangle(), m);
        if (!content.empty()) {
            // Append the content to the page's content. Surround the
            // original content with q...Q to the new content from the
            // page's original content.
            resources.mergeResources("<< /XObject << >> >>"_qpdf);
            resources.getKey("/XObject").replaceKey(name, stamp_fo);
            ph.addPageContents(inpdf.newStream("q\n"), true);
            ph.addPageContents(inpdf.newStream("\nQ\n" + content), false);
        }
        // Copy the annotations and form fields from the original page
        // to the new page. For more efficiency when copying multiple
        // pages, we can create a QPDFAcroFormDocumentHelper and pass
        // it in. See comments in QPDFPageObjectHelper.hh for details.
        ph.copyAnnotations(stamp_page_1, m);
    }

    QPDFWriter w(inpdf, outfile);
    w.setStaticID(true); // for testing only
    w.write();
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 4) {
        usage();
    }
    char const* infile = argv[1];
    char const* stampfile = argv[2];
    char const* outfile = argv[3];

    try {
        stamp_page(infile, stampfile, outfile);
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << std::endl;
        exit(2);
    }
    return 0;
}

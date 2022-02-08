#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFFileSpecObjectHelper.hh>

#include <iostream>
#include <cstring>

//
// This example attaches a file to an input file, adds a page to the
// beginning of the file that includes a file attachment annotation,
// and writes the result to an output file. It also illustrates a
// number of new API calls that were added in qpdf 10.2.
//

static char const* whoami = 0;

static void usage(std::string const& msg)
{
    std::cerr << msg << std::endl << std::endl
              << "Usage: " << whoami << " options" << std::endl
              << "Options:" << std::endl
              << " --infile infile.pdf" << std::endl
              << " --outfile outfile.pdf" << std::endl
              << " --attachment attachment" << std::endl
              << " [ --password infile-password ]" << std::endl
              << " [ --mimetype attachment mime type ]" << std::endl;
    exit(2);
}

static void process(char const* infilename, char const* password,
                    char const* attachment, char const* mimetype,
                    char const* outfilename)
{
    QPDF q;
    q.processFile(infilename, password);

    // Create an indirect object for the built-in Helvetica font. This
    // uses the qpdf literal syntax introduced in qpdf 10.6.
    auto f1 = q.makeIndirectObject(
        "<<"
        "  /Type /Font"
        "  /Subtype /Type1"
        "  /Name /F1"
        "  /BaseFont /Helvetica"
        "  /Encoding /WinAnsiEncoding"
        ">>"_qpdf);

    // Create a resources dictionary with fonts. This uses the new
    // parse introduced in qpdf 10.2 that takes a QPDF* and allows
    // indirect object references.
    auto resources = q.makeIndirectObject(
        QPDFObjectHandle::parse(
            &q,
            "<<"
            "  /Font <<"
            "    /F1 " + f1.unparse() +
            "  >>"
            ">>"));

    // Create a file spec.
    std::string key = QUtil::path_basename(attachment);
    std::cout << whoami << ": attaching " << attachment << " as " << key
              << std::endl;
    auto fs = QPDFFileSpecObjectHelper::createFileSpec(q, key, attachment);

    if (mimetype)
    {
        // Get an embedded file stream and set mimetype
        auto ef = QPDFEFStreamObjectHelper(fs.getEmbeddedFileStream());
        ef.setSubtype(mimetype);
    }

    // Add the embedded file at the document level as an attachment.
    auto efdh = QPDFEmbeddedFileDocumentHelper(q);
    efdh.replaceEmbeddedFile(key, fs);

    // Create a file attachment annotation.

    // Create appearance stream for the attachment.

    auto ap = QPDFObjectHandle::newStream(
        &q,
        "0 10 m\n"
        "10 0 l\n"
        "20 10 l\n"
        "10 0 m\n"
        "10 20 l\n"
        "0 0 20 20 re\n"
        "S\n");
    auto apdict = ap.getDict();
    apdict.replaceKey("/Resources", QPDFObjectHandle::newDictionary());
    apdict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    apdict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    apdict.replaceKey("/BBox", "[ 0 0 20 20 ]"_qpdf);
    auto annot = q.makeIndirectObject(
        QPDFObjectHandle::parse(
            &q,
            "<<"
            "  /AP <<"
            "    /N " + ap.unparse() +
            "  >>"
            "  /Contents "
            + QPDFObjectHandle::newUnicodeString(attachment).unparse() +
            "  /FS " + fs.getObjectHandle().unparse() +
            "  /NM " +
            QPDFObjectHandle::newUnicodeString(attachment).unparse() +
            "  /Rect [ 72 700 92 720 ]"
            "  /Subtype /FileAttachment"
            "  /Type /Annot"
            ">>"));

    // Generate contents for the page.
    auto contents = QPDFObjectHandle::newStream(
        &q,
        "q\n"
        "BT\n"
        "  102 700 Td\n"
        "  /F1 16 Tf\n"
        "  (Here is an attachment.) Tj\n"
        "ET\n"
        "Q\n");

    // Create the page object.
    auto page = QPDFObjectHandle::parse(
        &q,
        "<<"
        "  /Annots [ " + annot.unparse() + " ]"
        "  /Contents " + contents.unparse() +
        "  /MediaBox [0 0 612 792]"
        "  /Resources " + resources.unparse() +
        "  /Type /Page"
        ">>");

    // Add the page.
    q.addPage(page, true);

    QPDFWriter w(q, outfilename);
    w.setQDFMode(true);
    w.setSuppressOriginalObjectIDs(true);
    w.setDeterministicID(true);
    w.write();
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
        whoami += 3;
    }

    char const* infilename = 0;
    char const* password = 0;
    char const* attachment = 0;
    char const* outfilename = 0;
    char const* mimetype = 0;

    auto check_arg = [](char const* arg, std::string const& msg) {
        if (arg == nullptr)
        {
            usage(msg);
        }
    };

    for (int i = 1; i < argc; ++i)
    {
        char* arg = argv[i];
        char* next = argv[i+1];
        if (strcmp(arg, "--infile") == 0)
        {
            check_arg(next, "--infile takes an argument");
            infilename = next;
            ++i;
        }
        else if (strcmp(arg, "--password") == 0)
        {
            check_arg(next, "--password takes an argument");
            password = next;
            ++i;
        }
        else if (strcmp(arg, "--attachment") == 0)
        {
            check_arg(next, "--attachment takes an argument");
            attachment = next;
            ++i;
        }
        else if (strcmp(arg, "--outfile") == 0)
        {
            check_arg(next, "--outfile takes an argument");
            outfilename = next;
            ++i;
        }
        else if (strcmp(arg, "--mimetype") == 0)
        {
            check_arg(next, "--mimetype takes an argument");
            mimetype = next;
            ++i;
        }
        else
        {
            usage("unknown argument " + std::string(arg));
        }
    }
    if (! (infilename && attachment && outfilename))
    {
        usage("required arguments were not provided");
    }

    try
    {
        process(infilename, password, attachment, mimetype, outfilename);
    }
    catch (std::exception &e)
    {
        std::cerr << whoami << " exception: "
                  << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

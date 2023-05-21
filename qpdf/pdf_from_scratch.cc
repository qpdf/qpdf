#include <qpdf/QPDF.hh>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " n" << std::endl;
    exit(2);
}

static QPDFObjectHandle
createPageContents(QPDF& pdf, std::string const& text)
{
    return pdf.newStream("BT /F1 15 Tf 72 720 Td (" + text + ") Tj ET\n");
}

QPDFObjectHandle
newName(std::string const& name)
{
    return QPDFObjectHandle::newName(name);
}

void
runtest(int n)
{
    QPDF pdf;
    pdf.emptyPDF();
    if (n == 0) {
        // Create a minimal PDF from scratch.

        QPDFObjectHandle font =
            pdf.makeIndirectObject(QPDFObjectHandle::parse("<<"
                                                           " /Type /Font"
                                                           " /Subtype /Type1"
                                                           " /Name /F1"
                                                           " /BaseFont /Helvetica"
                                                           " /Encoding /WinAnsiEncoding"
                                                           ">>"));

        QPDFObjectHandle procset = pdf.makeIndirectObject(QPDFObjectHandle::parse("[/PDF /Text]"));

        QPDFObjectHandle contents = createPageContents(pdf, "First Page");

        QPDFObjectHandle mediabox = QPDFObjectHandle::parse("[0 0 612 792]");

        QPDFObjectHandle rfont = QPDFObjectHandle::newDictionary();
        rfont.replaceKey("/F1", font);

        QPDFObjectHandle resources = QPDFObjectHandle::newDictionary();
        resources.replaceKey("/ProcSet", procset);
        resources.replaceKey("/Font", rfont);

        QPDFObjectHandle page = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        page.replaceKey("/Type", newName("/Page"));
        page.replaceKey("/MediaBox", mediabox);
        page.replaceKey("/Contents", contents);
        page.replaceKey("/Resources", resources);

        QPDFPageDocumentHelper(pdf).addPage(page, true);

        QPDFWriter w(pdf, "a.pdf");
        w.setStaticID(true);
        w.setStreamDataMode(qpdf_s_preserve);
        w.write();
    } else {
        throw std::runtime_error(std::string("invalid test ") + std::to_string(n));
    }

    std::cout << "test " << n << " done" << std::endl;
}

int
main(int argc, char* argv[])
{
    QUtil::setLineBuf(stdout);
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if (argc != 2) {
        usage();
    }

    try {
        int n = QUtil::string_to_int(argv[1]);
        runtest(n);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

#include <qpdf/QPDF.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " n" << std::endl;
    exit(2);
}

static QPDFObjectHandle createPageContents(QPDF& pdf, std::string const& text)
{
    std::string contents = "BT /F1 15 Tf 72 720 Td (" + text + ") Tj ET\n";
    PointerHolder<Buffer> b = new Buffer(contents.length());
    unsigned char* bp = b->getBuffer();
    memcpy(bp, (char*)contents.c_str(), contents.length());
    return QPDFObjectHandle::newStream(&pdf, b);
}

QPDFObjectHandle newName(std::string const& name)
{
    return QPDFObjectHandle::newName(name);
}

void runtest(int n)
{
    QPDF pdf;
    pdf.emptyPDF();
    if (n == 0)
    {
        // Create a minimal PDF from scratch.

        std::map<std::string, QPDFObjectHandle> keys;
        std::vector<QPDFObjectHandle> items;

        keys.clear();
        keys["/Type"] = newName("/Font");
        keys["/Subtype"] = newName("/Type1");
        keys["/Name"] = newName("/F1");
        keys["/BaseFont"] = newName("/Helvetica");
        keys["/Encoding"] = newName("/WinAnsiEncoding");
        QPDFObjectHandle font = pdf.makeIndirectObject(
            QPDFObjectHandle::newDictionary(keys));

        items.clear();
        items.push_back(newName("/PDF"));
        items.push_back(newName("/Text"));
        QPDFObjectHandle procset = pdf.makeIndirectObject(
            QPDFObjectHandle::newArray(items));

        QPDFObjectHandle contents = createPageContents(pdf, "First Page");

        items.clear();
        items.push_back(QPDFObjectHandle::newInteger(0));
        items.push_back(QPDFObjectHandle::newInteger(0));
        items.push_back(QPDFObjectHandle::newInteger(612));
        items.push_back(QPDFObjectHandle::newInteger(792));
        QPDFObjectHandle mediabox = QPDFObjectHandle::newArray(items);

        keys.clear();
        keys["/F1"] = font;
        QPDFObjectHandle rfont = QPDFObjectHandle::newDictionary(keys);

        keys.clear();
        keys["/ProcSet"] = procset;
        keys["/Font"] = rfont;
        QPDFObjectHandle resources = QPDFObjectHandle::newDictionary(keys);

        keys.clear();
        keys["/Type"] = newName("/Page");
        keys["/MediaBox"] = mediabox;
        keys["/Contents"] = contents;
        keys["/Resources"] = resources;
        QPDFObjectHandle page = pdf.makeIndirectObject(
            QPDFObjectHandle::newDictionary(keys));

        pdf.addPage(page, true);

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else
    {
	throw std::runtime_error(std::string("invalid test ") +
				 QUtil::int_to_string(n));
    }

    std::cout << "test " << n << " done" << std::endl;
}

int main(int argc, char* argv[])
{
    QUtil::setLineBuf(stdout);
    if ((whoami = strrchr(argv[0], '/')) == NULL)
    {
	whoami = argv[0];
    }
    else
    {
	++whoami;
    }
    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if (argc != 2)
    {
	usage();
    }

    try
    {
	int n = atoi(argv[1]);
	runtest(n);
    }
    catch (std::exception& e)
    {
	std::cerr << e.what() << std::endl;
	exit(2);
    }

    return 0;
}

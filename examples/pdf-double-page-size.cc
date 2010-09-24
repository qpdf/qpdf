#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/QPDFWriter.hh>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " infile.pdf outfile.pdf [in-password]"
	      << std::endl
	      << "Double size of all pages in infile.pdf;"
	      << " write output to outfile.pdf" << std::endl;
    exit(2);
}

static void doubleBoxSize(QPDFObjectHandle& page, char const* box_name)
{
    // If there is a box of this name, replace it with a new box whose
    // elements are double the values of the original box.
    QPDFObjectHandle box = page.getKey(box_name);
    if (box.isNull())
    {
	return;
    }
    if (! (box.isArray() && (box.getArrayNItems() == 4)))
    {
	throw std::runtime_error(std::string("box ") + box_name +
				 " is not an array of four elements");
    }
    std::vector<QPDFObjectHandle> doubled;
    for (unsigned int i = 0; i < 4; ++i)
    {
	doubled.push_back(
	    QPDFObjectHandle::newReal(
		QUtil::double_to_string(
		    box.getArrayItem(i).getNumericValue() * 2.0, 2)));
    }
    page.replaceKey(box_name, QPDFObjectHandle::newArray(doubled));
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if (! ((argc == 3) || (argc == 4)))
    {
	usage();
    }

    char const* infilename = argv[1];
    char const* outfilename = argv[2];
    char const* password = (argc == 4) ? argv[3] : "";

    // Text to prepend to each page's contents
    char const* content = "2 0 0 2 0 0 cm\n";

    // Copy text into a buffer without the null terminator
    PointerHolder<Buffer> b = new Buffer(strlen(content));
    unsigned char* bp = b->getBuffer();
    memcpy(bp, (unsigned char*)content, strlen(content));

    try
    {
	QPDF qpdf;
	qpdf.processFile(infilename, password);

	std::vector<QPDFObjectHandle> pages = qpdf.getAllPages();
	for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	     iter != pages.end(); ++iter)
	{
	    QPDFObjectHandle& page = *iter;

	    // Prepend the buffer to the page's contents
	    page.addPageContents(QPDFObjectHandle::newStream(&qpdf, b), true);

	    // Double the size of each of the content boxes
	    doubleBoxSize(page, "/MediaBox");
	    doubleBoxSize(page, "/CropBox");
	    doubleBoxSize(page, "/BleedBox");
	    doubleBoxSize(page, "/TrimBox");
	    doubleBoxSize(page, "/ArtBox");
	}

	// Write out a new file
	QPDFWriter w(qpdf, outfilename);
	if (QUtil::get_env("IN_TESTSUITE"))
	{
	    // For the test suite, uncompress streams and use static
	    // IDs.
	    w.setStaticID(true);
	    w.setStreamDataMode(qpdf_s_uncompress);
	}
	w.write();
	std::cout << whoami << ": new file written to " << outfilename
		  << std::endl;
    }
    catch (std::exception &e)
    {
	std::cerr << whoami << " processing file " << infilename << ": "
		  << e.what() << std::endl;
	exit(2);
    }

    return 0;
}

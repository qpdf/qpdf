// This program tests miscellaneous functionality in the qpdf library
// that we don't want to pollute the qpdf program with.

#include <qpdf/QPDF.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QPDFWriter.hh>
#include <iostream>
#include <string.h>
#include <map>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " n filename" << std::endl;
    exit(2);
}

void runtest(int n, char const* filename)
{
    QPDF pdf;
    if (n == 0)
    {
	pdf.setAttemptRecovery(false);
    }
    pdf.processFile(filename);

    if ((n == 0) || (n == 1))
    {
	QPDFObjectHandle trailer = pdf.getTrailer();
	QPDFObjectHandle qtest = trailer.getKey("/QTest");

	if (! trailer.hasKey("/QTest"))
	{
	    // This will always happen when /QTest is null because
	    // hasKey returns false for null keys regardless of
	    // whether the key exists or not.  That way there's never
	    // any difference between a key that is present and null
	    // and a key that is absent.
	    QTC::TC("qpdf", "main QTest implicit");
	    std::cout << "/QTest is implicit" << std::endl;
	}

	QTC::TC("qpdf", "main QTest indirect",
		qtest.isIndirect() ? 1 : 0);
	std::cout << "/QTest is "
		  << (qtest.isIndirect() ? "in" : "")
		  << "direct" << std::endl;

	if (qtest.isNull())
	{
	    QTC::TC("qpdf", "main QTest null");
	    std::cout << "/QTest is null" << std::endl;
	}
	else if (qtest.isBool())
	{
	    QTC::TC("qpdf", "main QTest bool",
		    qtest.getBoolValue() ? 1 : 0);
	    std::cout << "/QTest is Boolean with value "
		      << (qtest.getBoolValue() ? "true" : "false")
		      << std::endl;
	}
	else if (qtest.isInteger())
	{
	    QTC::TC("qpdf", "main QTest int");
	    std::cout << "/QTest is an integer with value "
		      << qtest.getIntValue() << std::endl;
	}
	else if (qtest.isReal())
	{
	    QTC::TC("qpdf", "main QTest real");
	    std::cout << "/QTest is a real number with value "
		      << qtest.getRealValue() << std::endl;
	}
	else if (qtest.isName())
	{
	    QTC::TC("qpdf", "main QTest name");
	    std::cout << "/QTest is a name with value "
		      << qtest.getName() << std::endl;
	}
	else if (qtest.isString())
	{
	    QTC::TC("qpdf", "main QTest string");
	    std::cout << "/QTest is a string with value "
		      << qtest.getStringValue() << std::endl;
	}
	else if (qtest.isArray())
	{
	    QTC::TC("qpdf", "main QTest array");
	    int n = qtest.getArrayNItems();
	    std::cout << "/QTest is an array with "
		      << n << " items" << std::endl;
	    for (int i = 0; i < n; ++i)
	    {
		QTC::TC("qpdf", "main QTest array indirect",
			qtest.getArrayItem(i).isIndirect() ? 1 : 0);
		std::cout << "  item " << i << " is "
			  << (qtest.getArrayItem(i).isIndirect() ? "in" : "")
			  << "direct" << std::endl;
	    }
	}
	else if (qtest.isDictionary())
	{
	    QTC::TC("qpdf", "main QTest dictionary");
	    std::cout << "/QTest is a dictionary" << std::endl;
	    std::set<std::string> keys = qtest.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		QTC::TC("qpdf", "main QTest dictionary indirect",
			(qtest.getKey(*iter).isIndirect() ? 1 : 0));
		std::cout << "  " << *iter << " is "
			  << (qtest.getKey(*iter).isIndirect() ? "in" : "")
			  << "direct" << std::endl;
	    }
	}
	else if (qtest.isStream())
	{
	    QTC::TC("qpdf", "main QTest stream");
	    std::cout << "/QTest is a stream.  Dictionary: "
		      << qtest.getDict().unparse() << std::endl;

	    std::cout << "Raw stream data:" << std::endl;
	    std::cout.flush();
	    PointerHolder<Pl_StdioFile> out = new Pl_StdioFile("raw", stdout);
	    qtest.pipeStreamData(out.getPointer(), false, false, false);

	    std::cout << std::endl << "Uncompressed stream data:" << std::endl;
	    if (qtest.pipeStreamData(0, true, false, false))
	    {
		std::cout.flush();
		out = new Pl_StdioFile("filtered", stdout);
		qtest.pipeStreamData(out.getPointer(), true, false, false);
		std::cout << std::endl << "End of stream data" << std::endl;
	    }
	    else
	    {
		std::cout << "Stream data is not filterable." << std::endl;
	    }
	}
	else
	{
	    // Should not happen!
	    std::cout << "/QTest is an unknown object" << std::endl;
	}

	std::cout << "unparse: " << qtest.unparse() << std::endl
		  << "unparseResolved: " << qtest.unparseResolved()
		  << std::endl;
    }
    else if (n == 2)
    {
	// Encrypted file.  This test case is designed for a specific
	// PDF file.

	QPDFObjectHandle trailer = pdf.getTrailer();
	std::cout << trailer.getKey("/Info").
	    getKey("/CreationDate").getStringValue() << std::endl;
	std::cout << trailer.getKey("/Info").
	    getKey("/Producer").getStringValue() << std::endl;

	QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
	std::cout << encrypt.getKey("/O").unparse() << std::endl;
	std::cout << encrypt.getKey("/U").unparse() << std::endl;

	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle pages = root.getKey("/Pages");
	QPDFObjectHandle kids = pages.getKey("/Kids");
	QPDFObjectHandle page = kids.getArrayItem(1); // second page
	QPDFObjectHandle contents = page.getKey("/Contents");
	PointerHolder<Pl_StdioFile> out = new Pl_StdioFile("filtered", stdout);
	contents.pipeStreamData(out.getPointer(), true, false, false);
    }
    else if (n == 3)
    {
	QPDFObjectHandle streams = pdf.getTrailer().getKey("/QStreams");
	for (int i = 0; i < streams.getArrayNItems(); ++i)
	{
	    QPDFObjectHandle stream = streams.getArrayItem(i);
	    std::cout << "-- stream " << i << " --" << std::endl;
	    std::cout.flush();
	    PointerHolder<Pl_StdioFile> out =
		new Pl_StdioFile("tokenized stream", stdout);
	    stream.pipeStreamData(out.getPointer(), true, true, false);
	}
    }
    else if (n == 4)
    {
	// Mutability testing: Make /QTest direct recursively, then
	// copy to /Info.  Also make some other mutations so we can
	// tell the difference and ensure that the original /QTest
	// isn't effected.
	QPDFObjectHandle trailer = pdf.getTrailer();
	QPDFObjectHandle qtest = trailer.getKey("/QTest");
	qtest.makeDirect();
	qtest.removeKey("/Subject");
	qtest.replaceKey("/Author",
			 QPDFObjectHandle::newString("Mr. Potato Head"));
	// qtest.A and qtest.B.A were originally the same object.
	// They no longer are after makeDirect().  Mutate one of them
	// and ensure the other is not changed.
	qtest.getKey("/A").setArrayItem(1, QPDFObjectHandle::newInteger(5));
	trailer.replaceKey("/Info", pdf.makeIndirectObject(qtest));
	QPDFWriter w(pdf, 0);
	w.setQDFMode(true);
	w.setStaticID(true);
	w.write();

	// Prevent "done" message from getting appended
	exit(0);
    }
    else if (n == 5)
    {
	std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
	int pageno = 0;
	for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	     iter != pages.end(); ++iter)
	{
	    QPDFObjectHandle& page = *iter;
	    ++pageno;

	    std::cout << "page " << pageno << ":" << std::endl;

	    std::cout << "  images:" << std::endl;
	    std::map<std::string, QPDFObjectHandle> images =
		page.getPageImages();
	    for (std::map<std::string, QPDFObjectHandle>::iterator iter =
		     images.begin(); iter != images.end(); ++iter)
	    {
		std::string const& name = (*iter).first;
		QPDFObjectHandle image = (*iter).second;
		QPDFObjectHandle dict = image.getDict();
		int width = dict.getKey("/Width").getIntValue();
		int height = dict.getKey("/Height").getIntValue();
		std::cout << "    " << name
			  << ": " << width << " x " << height
			  << std::endl;
	    }

	    std::cout << "  content:" << std::endl;
	    std::vector<QPDFObjectHandle> content = page.getPageContents();
	    for (std::vector<QPDFObjectHandle>::iterator iter = content.begin();
		 iter != content.end(); ++iter)
	    {
		std::cout << "    " << (*iter).unparse() << std::endl;
	    }

	    std::cout << "end page " << pageno << std::endl;
	}

	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle qstrings = root.getKey("/QStrings");
	if (qstrings.isArray())
	{
	    std::cout << "QStrings:" << std::endl;
	    int n = qstrings.getArrayNItems();
	    for (int i = 0; i < n; ++i)
	    {
		std::cout << qstrings.getArrayItem(i).getUTF8Value()
			  << std::endl;
	    }
	}

	QPDFObjectHandle qnumbers = root.getKey("/QNumbers");
	if (qnumbers.isArray())
	{
	    std::cout << "QNumbers:" << std::endl;
	    int n = qnumbers.getArrayNItems();
	    for (int i = 0; i < n; ++i)
	    {
		std::cout << QUtil::double_to_string(
		    qnumbers.getArrayItem(i).getNumericValue(), 3)
			  << std::endl;
	    }
	}
    }
    else
    {
	throw QEXC::General(std::string("invalid test ") +
			    QUtil::int_to_string(n));
    }

    std::cout << "test " << n << " done" << std::endl;
}

int main(int argc, char* argv[])
{
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

    if (argc != 3)
    {
	usage();
    }

    try
    {
	int n = atoi(argv[1]);
	char const* filename = argv[2];
	runtest(n, filename);
    }
    catch (std::exception& e)
    {
	std::cerr << e.what() << std::endl;
	exit(2);
    }

    return 0;
}

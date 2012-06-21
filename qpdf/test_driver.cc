// This program tests miscellaneous functionality in the qpdf library
// that we don't want to pollute the qpdf program with.

#include <qpdf/QPDF.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/QPDFWriter.hh>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <map>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " n filename" << std::endl;
    exit(2);
}

class Provider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    Provider(PointerHolder<Buffer> b) :
	b(b),
	bad_length(false)
    {
    }
    virtual ~Provider()
    {
    }
    virtual void provideStreamData(int objid, int generation,
				   Pipeline* p)
    {
	p->write(b->getBuffer(), b->getSize());
	if (this->bad_length)
	{
	    unsigned char ch = ' ';
	    p->write(&ch, 1);
	}
	p->finish();
    }
    void badLength(bool v)
    {
	this->bad_length = v;
    }

  private:
    PointerHolder<Buffer> b;
    bool bad_length;
};

static void checkPageContents(QPDFObjectHandle page,
                              std::string const& wanted_string)
{
    PointerHolder<Buffer> b1 =
        page.getKey("/Contents").getStreamData();
    std::string contents = std::string((char *)(b1->getBuffer()));
    if (contents.find(wanted_string) == std::string::npos)
    {
        std::cout << "didn't find " << wanted_string << " in "
                  << contents << std::endl;
    }
}

static QPDFObjectHandle createPageContents(QPDF& pdf, std::string const& text)
{
    std::string contents = "BT /F1 15 Tf 72 720 Td (" + text + ") Tj ET\n";
    PointerHolder<Buffer> b = new Buffer(contents.length());
    unsigned char* bp = b->getBuffer();
    memcpy(bp, (char*)contents.c_str(), contents.length());
    return QPDFObjectHandle::newStream(&pdf, b);
}

void runtest(int n, char const* filename)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    QPDF pdf;
    PointerHolder<char> file_buf;
    FILE* filep = 0;
    if (n == 0)
    {
	pdf.setAttemptRecovery(false);
    }
    if (n % 2 == 0)
    {
        if (n % 4 == 0)
        {
	    QTC::TC("qpdf", "exercise processFile(name)");
            pdf.processFile(filename);
        }
        else
        {
	    QTC::TC("qpdf", "exercise processFile(FILE*)");
            filep = QUtil::fopen_wrapper(std::string("open ") + filename,
                                         fopen(filename, "rb"));
            pdf.processFile(filep);
        }
    }
    else
    {
        QTC::TC("qpdf", "exercise processMemoryFile");
	FILE* f = QUtil::fopen_wrapper(std::string("open ") + filename,
				       fopen(filename, "rb"));
	fseek(f, 0, SEEK_END);
	size_t size = (size_t) QUtil::ftell_off_t(f);
	fseek(f, 0, SEEK_SET);
	file_buf = PointerHolder<char>(true, new char[size]);
	char* buf_p = file_buf.getPointer();
	size_t bytes_read = 0;
	size_t len = 0;
	while ((len = fread(buf_p + bytes_read, 1, size - bytes_read, f)) > 0)
	{
	    bytes_read += len;
	}
	if (bytes_read != size)
	{
	    if (ferror(f))
	    {
		throw std::runtime_error(
		    std::string("failure reading file ") + filename +
		    " into memory: read " +
		    QUtil::int_to_string(bytes_read) + "; wanted " +
		    QUtil::int_to_string(size));
	    }
	    else
	    {
		throw std::logic_error(
		    std::string("premature eof reading file ") + filename +
		    " into memory: read " +
		    QUtil::int_to_string(bytes_read) + "; wanted " +
		    QUtil::int_to_string(size));
	    }
	}
	fclose(f);
	pdf.processMemoryFile(filename, buf_p, size);
    }

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
	    QUtil::binary_stdout();
	    PointerHolder<Pl_StdioFile> out = new Pl_StdioFile("raw", stdout);
	    qtest.pipeStreamData(out.getPointer(), false, false, false);

	    std::cout << std::endl << "Uncompressed stream data:" << std::endl;
	    if (qtest.pipeStreamData(0, true, false, false))
	    {
		std::cout.flush();
		QUtil::binary_stdout();
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
	QUtil::binary_stdout();
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
	    QUtil::binary_stdout();
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
	// and ensure the other is not changed.  These test cases are
	// crafted around a specific set of input files.
        QPDFObjectHandle A = qtest.getKey("/A");
        if (A.getArrayItem(0).getIntValue() == 1)
        {
            // Test mutators
            A.setArrayItem(1, QPDFObjectHandle::newInteger(5)); // 1 5 3
            A.insertItem(2, QPDFObjectHandle::newInteger(10)); // 1 5 10 3
            A.appendItem(QPDFObjectHandle::newInteger(12)); // 1 5 10 3 12
            A.eraseItem(3); // 1 5 10 12
            A.insertItem(4, QPDFObjectHandle::newInteger(6)); // 1 5 10 12 6
            A.insertItem(0, QPDFObjectHandle::newInteger(9)); // 9 1 5 10 12 6
        }
        else
        {
            std::vector<QPDFObjectHandle> items;
            items.push_back(QPDFObjectHandle::newInteger(14));
            items.push_back(QPDFObjectHandle::newInteger(15));
            items.push_back(QPDFObjectHandle::newInteger(9));
            A.setArrayFromVector(items);
        }

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
    else if (n == 6)
    {
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle metadata = root.getKey("/Metadata");
	if (! metadata.isStream())
	{
	    throw std::logic_error("test 6 run on file with no metadata");
	}
	Pl_Buffer bufpl("buffer");
	metadata.pipeStreamData(&bufpl, false, false, false);
	Buffer* buf = bufpl.getBuffer();
	unsigned char const* data = buf->getBuffer();
	bool cleartext = false;
	if ((buf->getSize() > 9) &&
	    (strncmp((char const*)data, "<?xpacket", 9) == 0))
	{
	    cleartext = true;
	}
	delete buf;
	std::cout << "encrypted="
		  << (pdf.isEncrypted() ? 1 : 0)
		  << "; cleartext="
		  << (cleartext ? 1 : 0)
		  << std::endl;
    }
    else if (n == 7)
    {
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle qstream = root.getKey("/QStream");
	if (! qstream.isStream())
	{
	    throw std::logic_error("test 7 run on file with no QStream");
	}
	PointerHolder<Buffer> b = new Buffer(20);
	unsigned char* bp = b->getBuffer();
	memcpy(bp, (char*)"new data for stream\n", 20); // no null!
	qstream.replaceStreamData(
	    b, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 8)
    {
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle qstream = root.getKey("/QStream");
	if (! qstream.isStream())
	{
	    throw std::logic_error("test 7 run on file with no QStream");
	}
	Pl_Buffer p1("buffer");
	Pl_Flate p2("compress", &p1, Pl_Flate::a_deflate);
	p2.write((unsigned char*)"new data for stream\n", 20); // no null!
	p2.finish();
	PointerHolder<Buffer> b = p1.getBuffer();
	// This is a bogus way to use StreamDataProvider, but it does
	// adequately test its functionality.
	Provider* provider = new Provider(b);
	PointerHolder<QPDFObjectHandle::StreamDataProvider> p = provider;
	qstream.replaceStreamData(
	    p, QPDFObjectHandle::newName("/FlateDecode"),
	    QPDFObjectHandle::newNull(), b->getSize());
	provider->badLength(true);
	try
	{
	    qstream.getStreamData();
	    std::cout << "oops -- getStreamData didn't throw" << std::endl;
	}
	catch (std::logic_error const& e)
	{
	    std::cout << "exception: " << e.what() << std::endl;
	}
	provider->badLength(false);
	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 9)
    {
	QPDFObjectHandle root = pdf.getRoot();
	PointerHolder<Buffer> b1 = new Buffer(20);
	unsigned char* bp = b1->getBuffer();
	memcpy(bp, (char*)"data for new stream\n", 20); // no null!
	QPDFObjectHandle qstream = QPDFObjectHandle::newStream(&pdf, b1);
	QPDFObjectHandle rstream = QPDFObjectHandle::newStream(&pdf);
	try
	{
	    rstream.getStreamData();
	    std::cout << "oops -- getStreamData didn't throw" << std::endl;
	}
	catch (std::logic_error const& e)
	{
	    std::cout << "exception: " << e.what() << std::endl;
	}
	PointerHolder<Buffer> b2 = new Buffer(22);
	bp = b2->getBuffer();
	memcpy(bp, (char*)"data for other stream\n", 22); // no null!
	rstream.replaceStreamData(
	    b2, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
	root.replaceKey("/QStream", qstream);
	root.replaceKey("/RStream", rstream);
	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 10)
    {
	PointerHolder<Buffer> b1 = new Buffer(37);
	unsigned char* bp = b1->getBuffer();
	memcpy(bp, (char*)"BT /F1 12 Tf 72 620 Td (Baked) Tj ET\n", 37);
	PointerHolder<Buffer> b2 = new Buffer(38);
	bp = b2->getBuffer();
	memcpy(bp, (char*)"BT /F1 18 Tf 72 520 Td (Mashed) Tj ET\n", 38);

	std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
	pages[0].addPageContents(QPDFObjectHandle::newStream(&pdf, b1), true);
	pages[0].addPageContents(QPDFObjectHandle::newStream(&pdf, b2), false);

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 11)
    {
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle qstream = root.getKey("/QStream");
	PointerHolder<Buffer> b1 = qstream.getStreamData();
	PointerHolder<Buffer> b2 = qstream.getRawStreamData();
	if ((b1->getSize() == 7) &&
	    (memcmp(b1->getBuffer(), "potato\n", 7) == 0))
	{
	    std::cout << "filtered stream data okay" << std::endl;
	}
	if ((b2->getSize() == 15) &&
	    (memcmp(b2->getBuffer(), "706F7461746F0A\n", 15) == 0))
	{
	    std::cout << "raw stream data okay" << std::endl;
	}
    }
    else if (n == 12)
    {
	pdf.setOutputStreams(0, 0);
	pdf.showLinearizationData();
    }
    else if (n == 13)
    {
	std::ostringstream out;
	std::ostringstream err;
	pdf.setOutputStreams(&out, &err);
	pdf.showLinearizationData();
	std::cout << "---output---" << std::endl
		  << out.str()
		  << "---error---" << std::endl
		  << err.str();
    }
    else if (n == 14)
    {
	// Exercise swap and replace.  This test case is designed for
	// a specific file.
	std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
	if (pages.size() != 4)
	{
	    throw std::logic_error("test " + QUtil::int_to_string(n) +
				   " not called 4-page file");
	}
	// Swap pages 2 and 3
	pdf.swapObjects(pages[1].getObjectID(), pages[1].getGeneration(),
			pages[2].getObjectID(), pages[2].getGeneration());
	// Replace object and swap objects
	QPDFObjectHandle trailer = pdf.getTrailer();
	QPDFObjectHandle qdict = trailer.getKey("/QDict");
	QPDFObjectHandle qarray = trailer.getKey("/QArray");
	// Force qdict but not qarray to resolve
	qdict.isDictionary();
	std::map<std::string, QPDFObjectHandle> dict_keys;
	dict_keys["/NewDict"] = QPDFObjectHandle::newInteger(2);
	QPDFObjectHandle new_dict = QPDFObjectHandle::newDictionary(dict_keys);
	try
	{
	    // Do it wrong first...
	    pdf.replaceObject(qdict.getObjectID(), qdict.getGeneration(),
			      qdict);
	}
	catch (std::logic_error)
	{
	    std::cout << "caught logic error as expected" << std::endl;
	}
	pdf.replaceObject(qdict.getObjectID(), qdict.getGeneration(),
			  new_dict);
	// Now qdict still points to the old dictionary
	std::cout << "old dict: " << qdict.getKey("/Dict").getIntValue()
		  << std::endl;
	// Swap dict and array
	pdf.swapObjects(qdict.getObjectID(), qdict.getGeneration(),
			qarray.getObjectID(), qarray.getGeneration());
	// Now qarray will resolve to new object but qdict is still
	// the old object
	std::cout << "old dict: " << qdict.getKey("/Dict").getIntValue()
		  << std::endl;
	std::cout << "new dict: " << qarray.getKey("/NewDict").getIntValue()
		  << std::endl;
	// Reread qdict, now pointing to an array
	qdict = pdf.getObjectByID(qdict.getObjectID(), qdict.getGeneration());
	std::cout << "swapped array: " << qdict.getArrayItem(0).getName()
		  << std::endl;

	// Exercise getAsMap and getAsArray
	std::vector<QPDFObjectHandle> array_elements =
	    qdict.getArrayAsVector();
	std::map<std::string, QPDFObjectHandle> dict_items =
	    qarray.getDictAsMap();
	if ((array_elements.size() == 1) &&
	    (array_elements[0].getName() == "/Array") &&
	    (dict_items.size() == 1) &&
	    (dict_items["/NewDict"].getIntValue() == 2))
	{
	    std::cout << "array and dictionary contents are correct"
		      << std::endl;
	}

	// Exercise writing to memory buffer
	QPDFWriter w(pdf);
	w.setOutputMemory();
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
	Buffer* b = w.getBuffer();
	FILE* f = QUtil::fopen_wrapper(std::string("open a.pdf"),
				       fopen("a.pdf", "wb"));
	fwrite(b->getBuffer(), b->getSize(), 1, f);
	fclose(f);
	delete b;
    }
    else if (n == 15)
    {
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
        // Reference to original page numbers for this test case are
        // numbered from 0.

        // Remove pages from various places, checking to make sure
        // that our pages reference is getting updated.
        assert(pages.size() == 10);
        pdf.removePage(pages.back()); // original page 9
        assert(pages.size() == 9);
        pdf.removePage(*pages.begin()); // original page 0
        assert(pages.size() == 8);
        checkPageContents(pages[4], "Original page 5");
        pdf.removePage(pages[4]); // original page 5
        assert(pages.size() == 7);
        checkPageContents(pages[4], "Original page 6");
        checkPageContents(pages[0], "Original page 1");
        checkPageContents(pages[6], "Original page 8");

        // Insert pages

        // Create some content streams.
        std::vector<QPDFObjectHandle> contents;
        contents.push_back(createPageContents(pdf, "New page 1"));
        contents.push_back(createPageContents(pdf, "New page 0"));
        contents.push_back(createPageContents(pdf, "New page 5"));
        contents.push_back(createPageContents(pdf, "New page 6"));
        contents.push_back(createPageContents(pdf, "New page 11"));
        contents.push_back(createPageContents(pdf, "New page 12"));

        // Create some page objects.  Start with an existing
        // dictionary and modify it.  Using the results of
        // getDictAsMap to create a new dictionary effectively creates
        // a shallow copy.
        std::map<std::string, QPDFObjectHandle> page_dict_keys =
            QPDFObjectHandle(pages[0]).getDictAsMap();
        std::vector<QPDFObjectHandle> new_pages;
        for (std::vector<QPDFObjectHandle>::iterator iter = contents.begin();
             iter != contents.end(); ++iter)
        {
            // We will retain indirect object references to other
            // indirect objects other than page content.
            page_dict_keys["/Contents"] = *iter;
            QPDFObjectHandle page =
                QPDFObjectHandle::newDictionary(page_dict_keys);
            if (iter == contents.begin())
            {
                // leave direct
                new_pages.push_back(page);
            }
            else
            {
                new_pages.push_back(pdf.makeIndirectObject(page));
            }
        }

        // Now insert the pages
        pdf.addPage(new_pages[0], true);
        checkPageContents(pages[0], "New page 1");
        pdf.addPageAt(new_pages[1], true, pages[0]);
        assert(pages[0].getObjectID() == new_pages[1].getObjectID());
        pdf.addPageAt(new_pages[2], true, pages[5]);
        assert(pages[5].getObjectID() == new_pages[2].getObjectID());
        pdf.addPageAt(new_pages[3], false, pages[5]);
        assert(pages[6].getObjectID() == new_pages[3].getObjectID());
        assert(pages.size() == 11);
        pdf.addPage(new_pages[4], false);
        assert(pages[11].getObjectID() == new_pages[4].getObjectID());
        pdf.addPageAt(new_pages[5], false, pages.back());
        assert(pages.size() == 13);
        checkPageContents(pages[0], "New page 0");
        checkPageContents(pages[1], "New page 1");
        checkPageContents(pages[5], "New page 5");
        checkPageContents(pages[6], "New page 6");
        checkPageContents(pages[11], "New page 11");
        checkPageContents(pages[12], "New page 12");

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 16)
    {
        // Insert a page manually and then update the cache.
        std::vector<QPDFObjectHandle> const& all_pages = pdf.getAllPages();

        QPDFObjectHandle contents = createPageContents(pdf, "New page 10");
        std::map<std::string, QPDFObjectHandle> page_dict_keys =
            QPDFObjectHandle(all_pages[0]).getDictAsMap();
        page_dict_keys["/Contents"] = contents;
        QPDFObjectHandle page =
            pdf.makeIndirectObject(
                QPDFObjectHandle::newDictionary(page_dict_keys));

        // Insert the page manually.
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle pages = root.getKey("/Pages");
	QPDFObjectHandle kids = pages.getKey("/Kids");
        page.replaceKey("/Parent", pages);
        pages.replaceKey(
            "/Count",
            QPDFObjectHandle::newInteger(1 + (int)all_pages.size()));
        kids.appendItem(page);
        assert(all_pages.size() == 10);
        pdf.updateAllPagesCache();
        assert(all_pages.size() == 11);
        assert(all_pages.back().getObjectID() == page.getObjectID());

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 17)
    {
        // The input file to this test case is broken to exercise an
        // error condition.
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
        pdf.removePage(pages[0]);
        std::cout << "you can't see this" << std::endl;
    }
    else if (n == 18)
    {
        // Remove a page and re-insert it in the same file.
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();

        // Remove pages from various places, checking to make sure
        // that our pages reference is getting updated.
        assert(pages.size() == 10);
        QPDFObjectHandle page5 = pages[5];
        pdf.removePage(page5);
        pdf.addPage(page5, false);
        assert(pages.size() == 10);
        assert(pages.back().getObjectID() == page5.getObjectID());

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 19)
    {
        // Remove a page and re-insert it in the same file.
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();

        // Try to insert a page that's already there.
        pdf.addPage(pages[5], false);
        std::cout << "you can't see this" << std::endl;
    }
    else
    {
	throw std::runtime_error(std::string("invalid test ") +
				 QUtil::int_to_string(n));
    }

    if (filep)
    {
        fclose(filep);
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

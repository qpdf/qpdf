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
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <map>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " n filename1 [arg2]"
              << std::endl;
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

class ParserCallbacks: public QPDFObjectHandle::ParserCallbacks
{
  public:
    virtual ~ParserCallbacks()
    {
    }

    virtual void handleObject(QPDFObjectHandle);
    virtual void handleEOF();
};

void
ParserCallbacks::handleObject(QPDFObjectHandle obj)
{
    if (obj.isName() && (obj.getName() == "/Abort"))
    {
        std::cout << "test suite: terminating parsing" << std::endl;
        terminateParsing();
    }
    std::cout << obj.getTypeName() << ": ";
    if (obj.isInlineImage())
    {
        // Exercise getTypeCode
        assert(obj.getTypeCode() == QPDFObject::ot_inlineimage);
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

static std::string getPageContents(QPDFObjectHandle page)
{
    PointerHolder<Buffer> b1 =
        page.getKey("/Contents").getStreamData();
    return std::string(
        reinterpret_cast<char *>(b1->getBuffer()), b1->getSize()) + "\0";
}

static void checkPageContents(QPDFObjectHandle page,
                              std::string const& wanted_string)
{
    std::string contents = getPageContents(page);
    if (contents.find(wanted_string) == std::string::npos)
    {
        std::cout << "didn't find " << wanted_string << " in "
                  << contents << std::endl;
    }
}

static QPDFObjectHandle createPageContents(QPDF& pdf, std::string const& text)
{
    std::string contents = "BT /F1 15 Tf 72 720 Td (" + text + ") Tj ET\n";
    return QPDFObjectHandle::newStream(&pdf, contents);
}

void runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    if (n == 0)
    {
        // Throw in some random test cases that don't fit anywhere
        // else.  This is in addition to whatever else is going on in
        // test 0.

        // The code to trim user passwords looks for 0x28 (which is
        // "(") since it marks the beginning of the padding.  Exercise
        // the code to make sure it skips over 0x28 characters that
        // aren't part of padding.
        std::string password(
            "1234567890123456789012(45678\x28\xbf\x4e\x5e");
        assert(password.length() == 32);
        QPDF::trim_user_password(password);
        assert(password == "1234567890123456789012(45678");

        QPDFObjectHandle uninitialized;
        assert(uninitialized.getTypeCode() == QPDFObject::ot_uninitialized);
        assert(strcmp(uninitialized.getTypeName(), "uninitialized") == 0);
    }

    QPDF pdf;
    PointerHolder<char> file_buf;
    FILE* filep = 0;
    if (n == 0)
    {
	pdf.setAttemptRecovery(false);
    }
    if (((n == 35) || (n == 36)) && (arg2 != 0))
    {
        // arg2 is password
	pdf.processFile(filename1, arg2);
    }
    else if (n % 2 == 0)
    {
        if (n % 4 == 0)
        {
	    QTC::TC("qpdf", "exercise processFile(name)");
            pdf.processFile(filename1);
        }
        else
        {
	    QTC::TC("qpdf", "exercise processFile(FILE*)");
            filep = QUtil::safe_fopen(filename1, "rb");
            pdf.processFile(filename1, filep, false);
        }
    }
    else
    {
        QTC::TC("qpdf", "exercise processMemoryFile");
	FILE* f = QUtil::safe_fopen(filename1, "rb");
	fseek(f, 0, SEEK_END);
	size_t size = QUtil::tell(f);
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
		    std::string("failure reading file ") + filename1 +
		    " into memory: read " +
		    QUtil::int_to_string(bytes_read) + "; wanted " +
		    QUtil::int_to_string(size));
	    }
	    else
	    {
		throw std::logic_error(
		    std::string("premature eof reading file ") + filename1 +
		    " into memory: read " +
		    QUtil::int_to_string(bytes_read) + "; wanted " +
		    QUtil::int_to_string(size));
	    }
	}
	fclose(f);
	pdf.processMemoryFile(filename1, buf_p, size);
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
		  << "direct and has type "
                  << qtest.getTypeName()
                  << " (" << qtest.getTypeCode() << ")" << std::endl;

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
	    (strncmp(reinterpret_cast<char const*>(data),
                     "<?xpacket", 9) == 0))
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
	qstream.replaceStreamData(
	    "new data for stream\n",
            QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
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
	p2.write(QUtil::unsigned_char_pointer("new data for stream\n"),
                 20); // no null!
	p2.finish();
	PointerHolder<Buffer> b = p1.getBuffer();
	// This is a bogus way to use StreamDataProvider, but it does
	// adequately test its functionality.
	Provider* provider = new Provider(b);
	PointerHolder<QPDFObjectHandle::StreamDataProvider> p = provider;
	qstream.replaceStreamData(
	    p, QPDFObjectHandle::newName("/FlateDecode"),
	    QPDFObjectHandle::newNull());
	provider->badLength(false);
	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
        // Linearize to force the provider to be called multiple times.
        w.setLinearization(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();

        // Every time a provider pipes stream data, it has to provide
        // the same amount of data.
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
    }
    else if (n == 9)
    {
	QPDFObjectHandle root = pdf.getRoot();
        // Explicitly exercise the Buffer version of newStream
	PointerHolder<Buffer> buf = new Buffer(20);
	unsigned char* bp = buf->getBuffer();
	memcpy(bp, "data for new stream\n", 20); // no null!
	QPDFObjectHandle qstream = QPDFObjectHandle::newStream(
            &pdf, buf);
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
	rstream.replaceStreamData(
	    "data for other stream\n",
            QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
	root.replaceKey("/QStream", qstream);
	root.replaceKey("/RStream", rstream);
	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 10)
    {
	std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
	pages.at(0).addPageContents(
            QPDFObjectHandle::newStream(
                &pdf, "BT /F1 12 Tf 72 620 Td (Baked) Tj ET\n"), true);
	pages.at(0).addPageContents(
            QPDFObjectHandle::newStream(
                &pdf, "BT /F1 18 Tf 72 520 Td (Mashed) Tj ET\n"), false);

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
	pdf.swapObjects(pages.at(1).getObjGen(), pages.at(2).getObjGen());
	// Replace object and swap objects
	QPDFObjectHandle trailer = pdf.getTrailer();
	QPDFObjectHandle qdict = trailer.getKey("/QDict");
	QPDFObjectHandle qarray = trailer.getKey("/QArray");
	// Force qdict but not qarray to resolve
	qdict.isDictionary();
	QPDFObjectHandle new_dict = QPDFObjectHandle::newDictionary();
	new_dict.replaceKey("/NewDict", QPDFObjectHandle::newInteger(2));
	try
	{
	    // Do it wrong first...
	    pdf.replaceObject(qdict.getObjGen(), qdict);
	}
	catch (std::logic_error)
	{
	    std::cout << "caught logic error as expected" << std::endl;
	}
	pdf.replaceObject(qdict.getObjGen(), new_dict);
	// Now qdict still points to the old dictionary
	std::cout << "old dict: " << qdict.getKey("/Dict").getIntValue()
		  << std::endl;
	// Swap dict and array
	pdf.swapObjects(qdict.getObjGen(), qarray.getObjGen());
	// Now qarray will resolve to new object but qdict is still
	// the old object
	std::cout << "old dict: " << qdict.getKey("/Dict").getIntValue()
		  << std::endl;
	std::cout << "new dict: " << qarray.getKey("/NewDict").getIntValue()
		  << std::endl;
	// Reread qdict, now pointing to an array
	qdict = pdf.getObjectByObjGen(qdict.getObjGen());
	std::cout << "swapped array: " << qdict.getArrayItem(0).getName()
		  << std::endl;

	// Exercise getAsMap and getAsArray
	std::vector<QPDFObjectHandle> array_elements =
	    qdict.getArrayAsVector();
	std::map<std::string, QPDFObjectHandle> dict_items =
	    qarray.getDictAsMap();
	if ((array_elements.size() == 1) &&
	    (array_elements.at(0).getName() == "/Array") &&
	    (dict_items.size() == 1) &&
	    (dict_items["/NewDict"].getIntValue() == 2))
	{
	    std::cout << "array and dictionary contents are correct"
		      << std::endl;
	}

	// Exercise writing to memory buffer
        for (int i = 0; i < 2; ++i)
        {
            QPDFWriter w(pdf);
            w.setOutputMemory();
            // Exercise setOutputMemory with and without static ID
            w.setStaticID(i == 0);
            w.setStreamDataMode(qpdf_s_preserve);
            w.write();
            Buffer* b = w.getBuffer();
            std::string const filename = (i == 0 ? "a.pdf" : "b.pdf");
            FILE* f = QUtil::safe_fopen(filename.c_str(), "wb");
            fwrite(b->getBuffer(), b->getSize(), 1, f);
            fclose(f);
            delete b;
        }
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
        checkPageContents(pages.at(4), "Original page 5");
        pdf.removePage(pages.at(4)); // original page 5
        assert(pages.size() == 7);
        checkPageContents(pages.at(4), "Original page 6");
        checkPageContents(pages.at(0), "Original page 1");
        checkPageContents(pages.at(6), "Original page 8");

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
        QPDFObjectHandle page_template = pages.at(0);
        std::vector<QPDFObjectHandle> new_pages;
        for (std::vector<QPDFObjectHandle>::iterator iter = contents.begin();
             iter != contents.end(); ++iter)
        {
            // We will retain indirect object references to other
            // indirect objects other than page content.
            QPDFObjectHandle page = page_template.shallowCopy();
            page.replaceKey("/Contents", *iter);
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
        pdf.addPage(new_pages.at(0), true);
        checkPageContents(pages.at(0), "New page 1");
        pdf.addPageAt(new_pages.at(1), true, pages.at(0));
        assert(pages.at(0).getObjGen() == new_pages.at(1).getObjGen());
        pdf.addPageAt(new_pages.at(2), true, pages.at(5));
        assert(pages.at(5).getObjGen() == new_pages.at(2).getObjGen());
        pdf.addPageAt(new_pages.at(3), false, pages.at(5));
        assert(pages.at(6).getObjGen() == new_pages.at(3).getObjGen());
        assert(pages.size() == 11);
        pdf.addPage(new_pages.at(4), false);
        assert(pages.at(11).getObjGen() == new_pages.at(4).getObjGen());
        pdf.addPageAt(new_pages.at(5), false, pages.back());
        assert(pages.size() == 13);
        checkPageContents(pages.at(0), "New page 0");
        checkPageContents(pages.at(1), "New page 1");
        checkPageContents(pages.at(5), "New page 5");
        checkPageContents(pages.at(6), "New page 6");
        checkPageContents(pages.at(11), "New page 11");
        checkPageContents(pages.at(12), "New page 12");

        // Exercise writing to FILE*
        FILE* out =  QUtil::safe_fopen("a.pdf", "wb");
	QPDFWriter w(pdf, "FILE* a.pdf", out, true);
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 16)
    {
        // Insert a page manually and then update the cache.
        std::vector<QPDFObjectHandle> const& all_pages = pdf.getAllPages();

        QPDFObjectHandle contents = createPageContents(pdf, "New page 10");
        QPDFObjectHandle page =
            pdf.makeIndirectObject(
                QPDFObjectHandle(all_pages.at(0)).shallowCopy());
        page.replaceKey("/Contents", contents);

        // Insert the page manually.
	QPDFObjectHandle root = pdf.getRoot();
	QPDFObjectHandle pages = root.getKey("/Pages");
	QPDFObjectHandle kids = pages.getKey("/Kids");
        page.replaceKey("/Parent", pages);
        pages.replaceKey(
            "/Count",
            QPDFObjectHandle::newInteger(1 + all_pages.size()));
        kids.appendItem(page);
        assert(all_pages.size() == 10);
        pdf.updateAllPagesCache();
        assert(all_pages.size() == 11);
        assert(all_pages.back().getObjGen() == page.getObjGen());

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
        pdf.removePage(pages.at(0));
        std::cout << "you can't see this" << std::endl;
    }
    else if (n == 18)
    {
        // Remove a page and re-insert it in the same file.
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();

        // Remove pages from various places, checking to make sure
        // that our pages reference is getting updated.
        assert(pages.size() == 10);
        QPDFObjectHandle page5 = pages.at(5);
        pdf.removePage(page5);
        pdf.addPage(page5, false);
        assert(pages.size() == 10);
        assert(pages.back().getObjGen() == page5.getObjGen());

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
        pdf.addPage(pages.at(5), false);
        std::cout << "you can't see this" << std::endl;
    }
    else if (n == 20)
    {
        // Shallow copy an array
	QPDFObjectHandle trailer = pdf.getTrailer();
	QPDFObjectHandle qtest = trailer.getKey("/QTest");
        QPDFObjectHandle copy = qtest.shallowCopy();
        // Append shallow copy of a scalar
        copy.appendItem(trailer.getKey("/Size").shallowCopy());
        trailer.replaceKey("/QTest2", copy);

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 21)
    {
        // Try to shallow copy a stream
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
        QPDFObjectHandle page = pages.at(0);
        QPDFObjectHandle contents = page.getKey("/Contents");
        contents.shallowCopy();
        std::cout << "you can't see this" << std::endl;
    }
    else if (n == 22)
    {
        // Try to remove a page we don't have
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
        QPDFObjectHandle page = pages.at(0);
        pdf.removePage(page);
        pdf.removePage(page);
        std::cout << "you can't see this" << std::endl;
    }
    else if (n == 23)
    {
        std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
        pdf.removePage(pages.back());
    }
    else if (n == 24)
    {
        // Test behavior of reserved objects
        QPDFObjectHandle res1 = QPDFObjectHandle::newReserved(&pdf);
        QPDFObjectHandle res2 = QPDFObjectHandle::newReserved(&pdf);
	QPDFObjectHandle trailer = pdf.getTrailer();
        trailer.replaceKey("Array1", res1);
        trailer.replaceKey("Array2", res2);

        QPDFObjectHandle array1 = QPDFObjectHandle::newArray();
        QPDFObjectHandle array2 = QPDFObjectHandle::newArray();
        array1.appendItem(res2);
        array1.appendItem(QPDFObjectHandle::newInteger(1));
        array2.appendItem(res1);
        array2.appendItem(QPDFObjectHandle::newInteger(2));
        // Make sure trying to ask questions about a reserved object
        // doesn't break it.
        if (res1.isArray())
        {
            std::cout << "oops -- res1 is an array" << std::endl;
        }
        if (res1.isReserved())
        {
            std::cout << "res1 is still reserved after checking if array"
                      << std::endl;
        }
        pdf.replaceReserved(res1, array1);
        if (res1.isReserved())
        {
            std::cout << "oops -- res1 is still reserved" << std::endl;
        }
        else
        {
            std::cout << "res1 is no longer reserved" << std::endl;
        }
        res1.assertArray();
        std::cout << "res1 is an array" << std::endl;

        try
        {
            res2.unparseResolved();
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::logic_error e)
        {
            std::cout << "logic error: " << e.what() << std::endl;
        }
        try
        {
            res2.makeDirect();
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::logic_error e)
        {
            std::cout << "logic error: " << e.what() << std::endl;
        }

        pdf.replaceReserved(res2, array2);

        res2.assertArray();
        std::cout << "res2 is an array" << std::endl;

        // Verify that the previously added reserved keys can be
        // dereferenced properly now
        int i1 = res1.getArrayItem(0).getArrayItem(1).getIntValue();
        int i2 = res2.getArrayItem(0).getArrayItem(1).getIntValue();
        if ((i1 == 2) && (i2 == 1))
        {
            std::cout << "circular access and lazy resolution worked" << std::endl;
        }

	QPDFWriter w(pdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 25)
    {
        // The copy object tests are designed to work with a specific
        // file.  Look at the test suite for the file, and look at the
        // file for comments about the file's structure.

        // Copy qtest without crossing page boundaries.  Should get O1
        // and O2 and their streams but not O3 or any other pages.

        assert(arg2 != 0);
        QPDF newpdf;
        newpdf.processFile(arg2);
        QPDFObjectHandle qtest = pdf.getTrailer().getKey("/QTest");
        newpdf.getTrailer().replaceKey(
            "/QTest", newpdf.copyForeignObject(qtest));

	QPDFWriter w(newpdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 26)
    {
        // Copy the O3 page using addPage.  Copy qtest without
        // crossing page boundaries.  In addition to previous results,
        // should get page O3 but no other pages including the page
        // that O3 points to.  Also, inherited object will have been
        // pushed down and will be preserved.

        assert(arg2 != 0);
        QPDF newpdf;
        newpdf.processFile(arg2);
        QPDFObjectHandle qtest = pdf.getTrailer().getKey("/QTest");
        QPDFObjectHandle O3 = qtest.getKey("/O3");
        newpdf.addPage(O3, false);
        newpdf.getTrailer().replaceKey(
            "/QTest", newpdf.copyForeignObject(qtest));

	QPDFWriter w(newpdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 27)
    {
        // Copy O3 and the page O3 refers to before copying qtest.
        // Should get qtest plus only the O3 page and the page that O3
        // points to.  Inherited objects should be preserved.

        assert(arg2 != 0);
        QPDF newpdf;
        newpdf.processFile(arg2);
        QPDFObjectHandle qtest = pdf.getTrailer().getKey("/QTest");
        QPDFObjectHandle O3 = qtest.getKey("/O3");
        newpdf.addPage(O3.getKey("/OtherPage"), false);
        newpdf.addPage(O3, false);
        newpdf.getTrailer().replaceKey(
            "/QTest", newpdf.copyForeignObject(qtest));

	QPDFWriter w(newpdf, "a.pdf");
	w.setStaticID(true);
	w.setStreamDataMode(qpdf_s_preserve);
	w.write();
    }
    else if (n == 28)
    {
        // Copy foreign object errors
        try
        {
            pdf.copyForeignObject(pdf.getTrailer().getKey("/QTest"));
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::logic_error e)
        {
            std::cout << "logic error: " << e.what() << std::endl;
        }
        try
        {
            pdf.copyForeignObject(QPDFObjectHandle::newInteger(1));
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::logic_error e)
        {
            std::cout << "logic error: " << e.what() << std::endl;
        }
    }
    else if (n == 29)
    {
        // Detect mixed objects in QPDFWriter
        assert(arg2 != 0);
        QPDF other;
        other.processFile(arg2);
        // Should use copyForeignObject instead
        other.getTrailer().replaceKey(
            "/QTest", pdf.getTrailer().getKey("/QTest"));

        try
        {
            QPDFWriter w(other, "a.pdf");
            w.write();
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::logic_error e)
        {
            std::cout << "logic error: " << e.what() << std::endl;
        }
    }
    else if (n == 30)
    {
        assert(arg2 != 0);
        QPDF encrypted;
        encrypted.processFile(arg2, "user");
        QPDFWriter w(pdf, "b.pdf");
	w.setStreamDataMode(qpdf_s_preserve);
        w.copyEncryptionParameters(encrypted);
	w.write();

        // Make sure the contents are actually the same
        QPDF final;
        final.processFile("b.pdf", "user");
        std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
        std::string orig_contents = getPageContents(pages.at(0));
        pages = final.getAllPages();
        std::string new_contents = getPageContents(pages.at(0));
        if (orig_contents != new_contents)
        {
            std::cout << "oops -- page contents don't match" << std::endl
                      << "original:\n" << orig_contents
                      << "new:\n" << new_contents
                      << std::endl;
        }
    }
    else if (n == 31)
    {
        // Test object parsing from a string.  The input file is not used.

        QPDFObjectHandle o1 =
            QPDFObjectHandle::parse(
                "[/name 16059 3.14159 false\n"
                " << /key true /other [ (string1) (string2) ] >> null]");
        std::cout << o1.unparse() << std::endl;
        QPDFObjectHandle o2 = QPDFObjectHandle::parse("   12345 \f  ");
        assert(o2.isInteger() && (o2.getIntValue() == 12345));
        try
        {
            QPDFObjectHandle::parse("[1 0 R]", "indirect test");
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::logic_error e)
        {
            std::cout << "logic error parsing indirect: " << e.what()
                      << std::endl;
        }
        try
        {
            QPDFObjectHandle::parse("0 trailing", "trailing test");
            std::cout << "oops -- didn't throw" << std::endl;
        }
        catch (std::runtime_error e)
        {
            std::cout << "trailing data: " << e.what()
                      << std::endl;
        }
    }
    else if (n == 32)
    {
        // Extra header text
        char const* filenames[] = {"a.pdf", "b.pdf", "c.pdf", "d.pdf"};
        for (int i = 0; i < 4; ++i)
        {
            bool linearized = ((i & 1) != 0);
            bool newline = ((i & 2) != 0);
            QPDFWriter w(pdf, filenames[i]);
            w.setStaticID(true);
            std::cout
                << "file: " << filenames[i] << std::endl
                << "linearized: " << (linearized ? "yes" : "no") << std::endl
                << "newline: " << (newline ? "yes" : "no") << std::endl;
            w.setLinearization(linearized);
            w.setExtraHeaderText(newline
                                 ? "%% Comment with newline\n"
                                 : "%% Comment\n% No newline");
            w.write();
        }
    }
    else if (n == 33)
    {
        // Test writing to a custom pipeline
        Pl_Buffer p("buffer");
        QPDFWriter w(pdf);
        w.setStaticID(true);
        w.setOutputPipeline(&p);
        w.write();
        PointerHolder<Buffer> b = p.getBuffer();
        FILE* f = QUtil::safe_fopen("a.pdf", "wb");
        fwrite(b->getBuffer(), b->getSize(), 1, f);
        fclose(f);
    }
    else if (n == 34)
    {
        // Look at Extensions dictionary
        std::cout << "version: " << pdf.getPDFVersion() << std::endl
                  << "extension level: " << pdf.getExtensionLevel() << std::endl
                  << pdf.getRoot().getKey("/Extensions").unparse() << std::endl;
    }
    else if (n == 35)
    {
        // Extract attachments

        std::map<std::string, PointerHolder<Buffer> > attachments;
        QPDFObjectHandle root = pdf.getRoot();
        QPDFObjectHandle names = root.getKey("/Names");
        QPDFObjectHandle embeddedFiles = names.getKey("/EmbeddedFiles");
        names = embeddedFiles.getKey("/Names");
        for (int i = 0; i < names.getArrayNItems(); ++i)
        {
            QPDFObjectHandle item = names.getArrayItem(i);
            if (item.isDictionary() &&
                item.getKey("/Type").isName() &&
                (item.getKey("/Type").getName() == "/Filespec") &&
                item.getKey("/EF").isDictionary() &&
                item.getKey("/EF").getKey("/F").isStream())
            {
                std::string filename = item.getKey("/F").getStringValue();
                QPDFObjectHandle stream = item.getKey("/EF").getKey("/F");
                attachments[filename] = stream.getStreamData();
            }
        }
        for (std::map<std::string, PointerHolder<Buffer> >::iterator iter =
                 attachments.begin(); iter != attachments.end(); ++iter)
        {
            std::string const& filename = (*iter).first;
            std::string data = std::string(
                reinterpret_cast<char const*>((*iter).second->getBuffer()),
                (*iter).second->getSize());
            bool is_binary = false;
            for (size_t i = 0; i < data.size(); ++i)
            {
                if ((data.at(i) < 0) || (data.at(i) > 126))
                {
                    is_binary = true;
                    break;
                }
            }
            if (is_binary)
            {
                std::string t;
                for (size_t i = 0;
                     i < std::min(data.size(), static_cast<size_t>(20));
                     ++i)
                {
                    if ((data.at(i) >= 32) && (data.at(i) <= 126))
                    {
                        t += data.at(i);
                    }
                    else
                    {
                        t += ".";
                    }
                }
                t += " (" + QUtil::int_to_string(data.size()) + " bytes)";
                data = t;
            }
            std::cout << filename << ":\n" << data << "--END--\n";
        }
    }
    else if (n == 36)
    {
        // Extract raw unfilterable attachment

        QPDFObjectHandle root = pdf.getRoot();
        QPDFObjectHandle names = root.getKey("/Names");
        QPDFObjectHandle embeddedFiles = names.getKey("/EmbeddedFiles");
        names = embeddedFiles.getKey("/Names");
        for (int i = 0; i < names.getArrayNItems(); ++i)
        {
            QPDFObjectHandle item = names.getArrayItem(i);
            if (item.isDictionary() &&
                item.getKey("/Type").isName() &&
                (item.getKey("/Type").getName() == "/Filespec") &&
                item.getKey("/EF").isDictionary() &&
                item.getKey("/EF").getKey("/F").isStream() &&
                (item.getKey("/F").getStringValue() == "attachment1.txt"))
            {
                std::string filename = item.getKey("/F").getStringValue();
                QPDFObjectHandle stream = item.getKey("/EF").getKey("/F");
                Pl_Buffer p1("buffer");
                Pl_Flate p2("compress", &p1, Pl_Flate::a_inflate);
                stream.pipeStreamData(&p2, false, false, false);
                PointerHolder<Buffer> buf = p1.getBuffer();
                std::string data = std::string(
                    reinterpret_cast<char const*>(buf->getBuffer()),
                    buf->getSize());
                std::cout << stream.getDict().unparse()
                          << filename << ":\n" << data << "--END--\n";
            }
        }
    }
    else if (n == 37)
    {
        // Parse content streams of all pages
        std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
        for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
             iter != pages.end(); ++iter)
        {
            QPDFObjectHandle page = *iter;
            QPDFObjectHandle contents = page.getKey("/Contents");
            ParserCallbacks cb;
            QPDFObjectHandle::parseContentStream(contents, &cb);
        }
    }
    else if (n == 38)
    {
        // Designed for override-compressed-object.pdf
        QPDFObjectHandle qtest = pdf.getRoot().getKey("/QTest");
        for (int i = 0; i < qtest.getArrayNItems(); ++i)
        {
            std::cout << qtest.getArrayItem(i).unparseResolved() << std::endl;
        }
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

    if ((argc < 3) || (argc > 4))
    {
	usage();
    }

    try
    {
	int n = atoi(argv[1]);
	char const* filename1 = argv[2];
        char const* arg2 = argv[3];
	runtest(n, filename1, arg2);
    }
    catch (std::exception& e)
    {
	std::cerr << e.what() << std::endl;
	exit(2);
    }

    return 0;
}

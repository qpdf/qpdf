#include <qpdf/assert_test.h>

// This program tests miscellaneous functionality in the qpdf library
// that we don't want to pollute the qpdf program with.

#include <qpdf/QPDF.hh>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <algorithm>
#include <iostream>
#include <limits.h>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QPDF_OBJECT_NOWARN
#include <qpdf/QPDFObject.hh>

static char const* whoami = 0;

void
usage()
{
    std::cerr << "Usage: " << whoami << " n filename1 [arg2]" << std::endl;
    exit(2);
}

// Derive from QPDFNumberTreeObjectHelper -- See test 61
class ExtendNameTree: public QPDFNameTreeObjectHelper
{
  public:
    ExtendNameTree(QPDFObjectHandle o, QPDF& q);
    virtual ~ExtendNameTree();
};

ExtendNameTree::ExtendNameTree(QPDFObjectHandle o, QPDF& q) :
    QPDFNameTreeObjectHelper(o, q)
{
}

ExtendNameTree::~ExtendNameTree()
{
    std::cout << "~ExtendNameTree called" << std::endl;
}

class Provider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    Provider(std::shared_ptr<Buffer> b) :
        b(b),
        bad_length(false)
    {
    }
    virtual ~Provider() = default;
    virtual void
    provideStreamData(int objid, int generation, Pipeline* p)
    {
        // Don't change signature to use QPDFObjGen const& to detect
        // problems forwarding to legacy implementations.
        p->write(b->getBuffer(), b->getSize());
        if (this->bad_length) {
            unsigned char ch = ' ';
            p->write(&ch, 1);
        }
        p->finish();
    }
    void
    badLength(bool v)
    {
        this->bad_length = v;
    }

  private:
    std::shared_ptr<Buffer> b;
    bool bad_length;
};

class ParserCallbacks: public QPDFObjectHandle::ParserCallbacks
{
  public:
    virtual ~ParserCallbacks() = default;
    virtual void contentSize(size_t size);
    virtual void handleObject(QPDFObjectHandle, size_t, size_t);
    virtual void handleEOF();
};

void
ParserCallbacks::contentSize(size_t size)
{
    std::cout << "content size: " << size << std::endl;
}

void
ParserCallbacks::handleObject(
    QPDFObjectHandle obj, size_t offset, size_t length)
{
    if (obj.isName() && (obj.getName() == "/Abort")) {
        std::cout << "test suite: terminating parsing" << std::endl;
        terminateParsing();
    }
    std::cout << obj.getTypeName() << ", offset=" << offset
              << ", length=" << length << ": ";
    if (obj.isInlineImage()) {
        // Exercise getTypeCode
        assert(obj.getTypeCode() == ::ot_inlineimage);
        std::cout << QUtil::hex_encode(obj.getInlineImageValue()) << std::endl;
    } else {
        std::cout << obj.unparse() << std::endl;
    }
}

void
ParserCallbacks::handleEOF()
{
    std::cout << "-EOF-" << std::endl;
}

class TokenFilter: public QPDFObjectHandle::TokenFilter
{
  public:
    TokenFilter() = default;
    virtual ~TokenFilter() = default;
    virtual void
    handleToken(QPDFTokenizer::Token const& t)
    {
        if (t == QPDFTokenizer::Token(QPDFTokenizer::tt_string, "Potato")) {
            // Exercise unparsing of strings by token constructor
            writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_string, "Salad"));
        } else {
            writeToken(t);
        }
    }
    virtual void
    handleEOF()
    {
        writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_name, "/bye"));
        write("\n");
    }
};

static std::string
getPageContents(QPDFObjectHandle page)
{
    std::shared_ptr<Buffer> b1 = page.getKey("/Contents").getStreamData();
    return std::string(
               reinterpret_cast<char*>(b1->getBuffer()), b1->getSize()) +
        "\0";
}

static void
checkPageContents(QPDFObjectHandle page, std::string const& wanted_string)
{
    std::string contents = getPageContents(page);
    if (contents.find(wanted_string) == std::string::npos) {
        std::cout << "didn't find " << wanted_string << " in " << contents
                  << std::endl;
    }
}

static QPDFObjectHandle
createPageContents(QPDF& pdf, std::string const& text)
{
    std::string contents = "BT /F1 15 Tf 72 720 Td (" + text + ") Tj ET\n";
    return QPDFObjectHandle::newStream(&pdf, contents);
}

static void
print_rect(std::ostream& out, QPDFObjectHandle::Rectangle const& r)
{
    out << "[" << r.llx << ", " << r.lly << ", " << r.urx << ", " << r.ury
        << "]";
}

#define assert_compare_numbers(expected, expr) \
 compare_numbers(#expr, expected, expr)

template <typename T1, typename T2>
static void
compare_numbers(char const* description, T1 const& expected, T2 const& actual)
{
    if (expected != actual) {
        std::cerr << description << ": expected = " << expected
                  << "; actual = " << actual << std::endl;
    }
}

static void
test_0_1(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle trailer = pdf.getTrailer();
    QPDFObjectHandle qtest = trailer.getKey("/QTest");

    if (!trailer.hasKey("/QTest")) {
        // This will always happen when /QTest is null because
        // hasKey returns false for null keys regardless of
        // whether the key exists or not.  That way there's never
        // any difference between a key that is present and null
        // and a key that is absent.
        QTC::TC("qpdf", "main QTest implicit");
        std::cout << "/QTest is implicit" << std::endl;
    }

    QTC::TC("qpdf", "main QTest indirect", qtest.isIndirect() ? 1 : 0);
    std::cout << "/QTest is " << (qtest.isIndirect() ? "in" : "")
              << "direct and has type " << qtest.getTypeName() << " ("
              << qtest.getTypeCode() << ")" << std::endl;

    if (qtest.isNull()) {
        QTC::TC("qpdf", "main QTest null");
        std::cout << "/QTest is null" << std::endl;
    } else if (qtest.isBool()) {
        QTC::TC("qpdf", "main QTest bool", qtest.getBoolValue() ? 1 : 0);
        std::cout << "/QTest is Boolean with value "
                  << (qtest.getBoolValue() ? "true" : "false") << std::endl;
    } else if (qtest.isInteger()) {
        QTC::TC("qpdf", "main QTest int");
        std::cout << "/QTest is an integer with value " << qtest.getIntValue()
                  << std::endl;
    } else if (qtest.isReal()) {
        QTC::TC("qpdf", "main QTest real");
        std::cout << "/QTest is a real number with value "
                  << qtest.getRealValue() << std::endl;
    } else if (qtest.isName()) {
        QTC::TC("qpdf", "main QTest name");
        std::cout << "/QTest is a name with value " << qtest.getName()
                  << std::endl;
    } else if (qtest.isString()) {
        QTC::TC("qpdf", "main QTest string");
        std::cout << "/QTest is a string with value " << qtest.getStringValue()
                  << std::endl;
    } else if (qtest.isArray()) {
        QTC::TC("qpdf", "main QTest array");
        std::cout << "/QTest is an array with " << qtest.getArrayNItems()
                  << " items" << std::endl;
        int i = 0;
        for (auto& iter: qtest.aitems()) {
            QTC::TC(
                "qpdf", "main QTest array indirect", iter.isIndirect() ? 1 : 0);
            std::cout << "  item " << i << " is "
                      << (iter.isIndirect() ? "in" : "") << "direct"
                      << std::endl;
            ++i;
        }
    } else if (qtest.isDictionary()) {
        QTC::TC("qpdf", "main QTest dictionary");
        std::cout << "/QTest is a dictionary" << std::endl;
        for (auto& iter: qtest.ditems()) {
            QTC::TC(
                "qpdf",
                "main QTest dictionary indirect",
                iter.second.isIndirect() ? 1 : 0);
            std::cout << "  " << iter.first << " is "
                      << (iter.second.isIndirect() ? "in" : "") << "direct"
                      << std::endl;
        }
    } else if (qtest.isStream()) {
        QTC::TC("qpdf", "main QTest stream");
        std::cout << "/QTest is a stream.  Dictionary: "
                  << qtest.getDict().unparse() << std::endl;

        std::cout << "Raw stream data:" << std::endl;
        std::cout.flush();
        QUtil::binary_stdout();
        auto out = std::make_shared<Pl_StdioFile>("raw", stdout);
        qtest.pipeStreamData(out.get(), 0, qpdf_dl_none);

        std::cout << std::endl << "Uncompressed stream data:" << std::endl;
        if (qtest.pipeStreamData(0, 0, qpdf_dl_all)) {
            std::cout.flush();
            QUtil::binary_stdout();
            out = std::make_shared<Pl_StdioFile>("filtered", stdout);
            qtest.pipeStreamData(out.get(), 0, qpdf_dl_all);
            std::cout << std::endl << "End of stream data" << std::endl;
        } else {
            std::cout << "Stream data is not filterable." << std::endl;
        }
    } else {
        // Should not happen!
        std::cout << "/QTest is an unknown object" << std::endl;
    }

    std::cout << "unparse: " << qtest.unparse() << std::endl
              << "unparseResolved: " << qtest.unparseResolved() << std::endl;
}

static void
test_2(QPDF& pdf, char const* arg2)
{
    // Encrypted file.  This test case is designed for a specific
    // PDF file.

    QPDFObjectHandle trailer = pdf.getTrailer();
    std::cout
        << trailer.getKey("/Info").getKey("/CreationDate").getStringValue()
        << std::endl;
    std::cout << trailer.getKey("/Info").getKey("/Producer").getStringValue()
              << std::endl;

    QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
    std::cout << encrypt.getKey("/O").unparse() << std::endl;
    std::cout << encrypt.getKey("/U").unparse() << std::endl;

    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle pages = root.getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");
    QPDFObjectHandle page = kids.getArrayItem(1); // second page
    QPDFObjectHandle contents = page.getKey("/Contents");
    QUtil::binary_stdout();
    auto out = std::make_shared<Pl_StdioFile>("filtered", stdout);
    contents.pipeStreamData(out.get(), 0, qpdf_dl_generalized);
}

static void
test_3(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle streams = pdf.getTrailer().getKey("/QStreams");
    for (int i = 0; i < streams.getArrayNItems(); ++i) {
        QPDFObjectHandle stream = streams.getArrayItem(i);
        std::cout << "-- stream " << i << " --" << std::endl;
        std::cout.flush();
        QUtil::binary_stdout();
        auto out = std::make_shared<Pl_StdioFile>("tokenized stream", stdout);
        stream.pipeStreamData(
            out.get(), qpdf_ef_normalize, qpdf_dl_generalized);
    }
}

static void
test_4(QPDF& pdf, char const* arg2)
{
    // Mutability testing: Make /QTest direct recursively, then
    // copy to /Info.  Also make some other mutations so we can
    // tell the difference and ensure that the original /QTest
    // isn't effected.
    QPDFObjectHandle trailer = pdf.getTrailer();
    QPDFObjectHandle qtest = trailer.getKey("/QTest");
    qtest.makeDirect();
    qtest.removeKey("/Subject");
    qtest.replaceKey("/Author", QPDFObjectHandle::newString("Mr. Potato Head"));
    // qtest.A and qtest.B.A were originally the same object.
    // They no longer are after makeDirect().  Mutate one of them
    // and ensure the other is not changed.  These test cases are
    // crafted around a specific set of input files.
    QPDFObjectHandle A = qtest.getKey("/A");
    if (A.getArrayItem(0).getIntValue() == 1) {
        // Test mutators
        A.setArrayItem(1, QPDFObjectHandle::newInteger(5)); // 1 5 3
        A.insertItem(2, QPDFObjectHandle::newInteger(10));  // 1 5 10 3
        A.appendItem(QPDFObjectHandle::newInteger(12));     // 1 5 10 3 12
        A.eraseItem(3);                                     // 1 5 10 12
        A.insertItem(4, QPDFObjectHandle::newInteger(6));   // 1 5 10 12 6
        A.insertItem(0, QPDFObjectHandle::newInteger(9));   // 9 1 5 10 12 6
    } else {
        std::vector<QPDFObjectHandle> items;
        items.push_back(QPDFObjectHandle::newInteger(14));
        items.push_back(QPDFObjectHandle::newInteger(15));
        items.push_back(QPDFObjectHandle::newInteger(9));
        A.setArrayFromVector(items);
    }

    QPDFObjectHandle qtest2 = trailer.getKey("/QTest2");
    if (!qtest2.isNull()) {
        // Test allow_streams=true
        qtest2.makeDirect(true);
        trailer.replaceKey("/QTest2", qtest2);
    }

    trailer.replaceKey("/Info", pdf.makeIndirectObject(qtest));
    QPDFWriter w(pdf, 0);
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();

    // Prevent "done" message from getting appended
    exit(0);
}

static void
test_5(QPDF& pdf, char const* arg2)
{
    int pageno = 0;
    for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        ++pageno;
        std::cout << "page " << pageno << ":" << std::endl;
        std::cout << "  images:" << std::endl;
        for (auto const& iter2: page.getImages()) {
            std::string const& name = iter2.first;
            QPDFObjectHandle image = iter2.second;
            QPDFObjectHandle dict = image.getDict();
            long long width = dict.getKey("/Width").getIntValue();
            long long height = dict.getKey("/Height").getIntValue();
            std::cout << "    " << name << ": " << width << " x " << height
                      << std::endl;
        }

        std::cout << "  content:" << std::endl;
        std::vector<QPDFObjectHandle> content = page.getPageContents();
        for (auto& iter2: content) {
            std::cout << "    " << iter2.unparse() << std::endl;
        }

        std::cout << "end page " << pageno << std::endl;
    }

    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle qstrings = root.getKey("/QStrings");
    if (qstrings.isArray()) {
        std::cout << "QStrings:" << std::endl;
        int nitems = qstrings.getArrayNItems();
        for (int i = 0; i < nitems; ++i) {
            std::cout << qstrings.getArrayItem(i).getUTF8Value() << std::endl;
        }
    }

    QPDFObjectHandle qnumbers = root.getKey("/QNumbers");
    if (qnumbers.isArray()) {
        std::cout << "QNumbers:" << std::endl;
        int nitems = qnumbers.getArrayNItems();
        for (int i = 0; i < nitems; ++i) {
            std::cout << QUtil::double_to_string(
                             qnumbers.getArrayItem(i).getNumericValue(),
                             3,
                             false)
                      << std::endl;
        }
    }
}

static void
test_6(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle metadata = root.getKey("/Metadata");
    if (!metadata.isStream()) {
        throw std::logic_error("test 6 run on file with no metadata");
    }
    std::string buf;
    Pl_String bufpl("buffer", nullptr, buf);
    metadata.pipeStreamData(&bufpl, 0, qpdf_dl_none);
    bool cleartext = false;
    if (buf.substr(0, 9) == "<?xpacket") {
        cleartext = true;
    }
    std::cout << "encrypted=" << (pdf.isEncrypted() ? 1 : 0)
              << "; cleartext=" << (cleartext ? 1 : 0) << std::endl;
}

static void
test_7(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle qstream = root.getKey("/QStream");
    if (!qstream.isStream()) {
        throw std::logic_error("test 7 run on file with no QStream");
    }
    qstream.replaceStreamData(
        "new data for stream\n",
        QPDFObjectHandle::newNull(),
        QPDFObjectHandle::newNull());
    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_8(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle qstream = root.getKey("/QStream");
    if (!qstream.isStream()) {
        throw std::logic_error("test 7 run on file with no QStream");
    }
    Pl_Buffer p1("buffer");
    Pl_Flate p2("compress", &p1, Pl_Flate::a_deflate);
    p2 << "new data for stream\n";
    p2.finish();
    auto b = p1.getBufferSharedPointer();
    // This is a bogus way to use StreamDataProvider, but it does
    // adequately test its functionality.
    Provider* provider = new Provider(b);
    auto p = std::shared_ptr<QPDFObjectHandle::StreamDataProvider>(provider);
    qstream.replaceStreamData(
        p,
        QPDFObjectHandle::newName("/FlateDecode"),
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
    try {
        qstream.getStreamData();
        std::cout << "oops -- getStreamData didn't throw" << std::endl;
    } catch (std::exception const& e) {
        std::cout << "exception: " << e.what() << std::endl;
    }
}

static void
test_9(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle root = pdf.getRoot();
    // Explicitly exercise the Buffer version of newStream
    auto buf = std::make_shared<Buffer>(20U);
    unsigned char* bp = buf->getBuffer();
    memcpy(bp, "data for new stream\n", 20); // no null!
    QPDFObjectHandle qstream = QPDFObjectHandle::newStream(&pdf, buf);
    QPDFObjectHandle rstream = QPDFObjectHandle::newStream(&pdf);
    try {
        rstream.getStreamData();
        std::cout << "oops -- getStreamData didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "exception: " << e.what() << std::endl;
    }
    rstream.replaceStreamData(
        "data for other stream\n",
        QPDFObjectHandle::newNull(),
        QPDFObjectHandle::newNull());
    root.replaceKey("/QStream", qstream);
    root.replaceKey("/RStream", rstream);
    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_10(QPDF& pdf, char const* arg2)
{
    std::vector<QPDFPageObjectHelper> pages =
        QPDFPageDocumentHelper(pdf).getAllPages();
    QPDFPageObjectHelper& ph(pages.at(0));
    ph.addPageContents(
        QPDFObjectHandle::newStream(
            &pdf, "BT /F1 12 Tf 72 620 Td (Baked) Tj ET\n"),
        true);
    ph.addPageContents(
        QPDFObjectHandle::newStream(
            &pdf, "BT /F1 18 Tf 72 520 Td (Mashed) Tj ET\n"),
        false);

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_11(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle qstream = root.getKey("/QStream");
    std::shared_ptr<Buffer> b1 = qstream.getStreamData();
    std::shared_ptr<Buffer> b2 = qstream.getRawStreamData();
    if ((b1->getSize() == 7) && (memcmp(b1->getBuffer(), "potato\n", 7) == 0)) {
        std::cout << "filtered stream data okay" << std::endl;
    }
    if ((b2->getSize() == 15) &&
        (memcmp(b2->getBuffer(), "706F7461746F0A\n", 15) == 0)) {
        std::cout << "raw stream data okay" << std::endl;
    }
}

static void
test_12(QPDF& pdf, char const* arg2)
{
#ifdef _MSC_VER
# pragma warning(disable : 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    pdf.setOutputStreams(0, 0);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
    pdf.showLinearizationData();
}

static void
test_13(QPDF& pdf, char const* arg2)
{
    std::ostringstream out;
    std::ostringstream err;
#ifdef _MSC_VER
# pragma warning(disable : 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    pdf.setOutputStreams(&out, &err);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
    pdf.showLinearizationData();
    std::cout << "---output---" << std::endl
              << out.str() << "---error---" << std::endl
              << err.str();
}

static void
test_14(QPDF& pdf, char const* arg2)
{
    // Exercise swap and replace.  This test case is designed for
    // a specific file.
    std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
    if (pages.size() != 4) {
        throw std::logic_error("test 14 not called 4-page file");
    }
    // Swap pages 2 and 3
    auto orig_page2 = pages.at(1);
    auto orig_page3 = pages.at(2);
    assert(orig_page2.getKey("/OrigPage").getIntValue() == 2);
    assert(orig_page3.getKey("/OrigPage").getIntValue() == 3);
    pdf.swapObjects(orig_page2.getObjGen(), orig_page3.getObjGen());
    assert(orig_page2.getKey("/OrigPage").getIntValue() == 3);
    assert(orig_page3.getKey("/OrigPage").getIntValue() == 2);
    // Replace object and swap objects
    QPDFObjectHandle trailer = pdf.getTrailer();
    QPDFObjectHandle qdict = trailer.getKey("/QDict");
    QPDFObjectHandle qarray = trailer.getKey("/QArray");
    // Force qdict but not qarray to resolve
    qdict.isDictionary();
    QPDFObjectHandle new_dict = QPDFObjectHandle::newDictionary();
    new_dict.replaceKey("/NewDict", QPDFObjectHandle::newInteger(2));
    try {
        // Do it wrong first...
        pdf.replaceObject(qdict.getObjGen(), qdict);
    } catch (std::logic_error const&) {
        std::cout << "caught logic error as expected" << std::endl;
    }
    pdf.replaceObject(qdict.getObjGen(), new_dict);
    // Now qdict points to the new dictionary
    std::cout << "old dict: " << qdict.getKey("/NewDict").getIntValue()
              << std::endl;
    // Swap dict and array
    pdf.swapObjects(qdict.getObjGen(), qarray.getObjGen());
    // Now qarray will resolve to new object and qdict resolves to
    // the array
    std::cout << "swapped array: " << qdict.getArrayItem(0).getName()
              << std::endl;
    std::cout << "new dict: " << qarray.getKey("/NewDict").getIntValue()
              << std::endl;
    // Reread qdict, still pointing to an array
    qdict = pdf.getObjectByObjGen(qdict.getObjGen());
    std::cout << "swapped array: " << qdict.getArrayItem(0).getName()
              << std::endl;

    // Exercise getAsMap and getAsArray
    std::vector<QPDFObjectHandle> array_elements = qdict.getArrayAsVector();
    std::map<std::string, QPDFObjectHandle> dict_items = qarray.getDictAsMap();
    if ((array_elements.size() == 1) &&
        (array_elements.at(0).getName() == "/Array") &&
        (dict_items.size() == 1) &&
        (dict_items["/NewDict"].getIntValue() == 2)) {
        std::cout << "array and dictionary contents are correct" << std::endl;
    }

    // Exercise writing to memory buffer
    for (int i = 0; i < 2; ++i) {
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

static void
test_15(QPDF& pdf, char const* arg2)
{
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
    // Reference to original page numbers for this test case are
    // numbered from 0.

    // Remove pages from various places, checking to make sure
    // that our pages reference is getting updated.
    assert(pages.size() == 10);
    assert(!pdf.everPushedInheritedAttributesToPages());
    pdf.removePage(pages.back()); // original page 9
    assert(pdf.everPushedInheritedAttributesToPages());
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
    bool first = true;
    for (auto const& iter: contents) {
        // We will retain indirect object references to other
        // indirect objects other than page content.
        QPDFObjectHandle page = page_template.shallowCopy();
        page.replaceKey("/Contents", iter);
        if (first) {
            // leave direct
            first = false;
        } else {
            page = pdf.makeIndirectObject(page);
        }
        new_pages.push_back(page);
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
    FILE* out = QUtil::safe_fopen("a.pdf", "wb");
    QPDFWriter w(pdf, "FILE* a.pdf", out, true);
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_16(QPDF& pdf, char const* arg2)
{
    // Insert a page manually and then update the cache.
    assert(!pdf.everCalledGetAllPages());
    std::vector<QPDFObjectHandle> const& all_pages = pdf.getAllPages();
    assert(pdf.everCalledGetAllPages());

    QPDFObjectHandle contents = createPageContents(pdf, "New page 10");
    QPDFObjectHandle page =
        pdf.makeIndirectObject(QPDFObjectHandle(all_pages.at(0)).shallowCopy());
    page.replaceKey("/Contents", contents);

    // Insert the page manually.
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle pages = root.getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");
    page.replaceKey("/Parent", pages);
    pages.replaceKey(
        "/Count",
        QPDFObjectHandle::newInteger(1 + QIntC::to_longlong(all_pages.size())));
    kids.appendItem(page);
    assert(all_pages.size() == 10);
    pdf.updateAllPagesCache();
    assert(pdf.everCalledGetAllPages());
    assert(all_pages.size() == 11);
    assert(all_pages.back().getObjGen() == page.getObjGen());

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_17(QPDF& pdf, char const* arg2)
{
    // The input file to this test case has a duplicated page.
    QPDFObjectHandle page_kids = pdf.getRoot().getKey("/Pages").getKey("/Kids");
    assert(
        page_kids.getArrayItem(0).getObjGen() ==
        page_kids.getArrayItem(1).getObjGen());
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
    assert(pages.size() == 3);
    assert(!(pages.at(0).getObjGen() == pages.at(1).getObjGen()));
    assert(
        QPDFObjectHandle(pages.at(0)).getKey("/Contents").getObjGen() ==
        QPDFObjectHandle(pages.at(1)).getKey("/Contents").getObjGen());
    pdf.removePage(pages.at(0));
    assert(pages.size() == 2);
    std::shared_ptr<Buffer> b =
        QPDFObjectHandle(pages.at(0)).getKey("/Contents").getStreamData();
    std::string contents = std::string(
        reinterpret_cast<char const*>(b->getBuffer()), b->getSize());
    assert(contents.find("page 0") != std::string::npos);
}

static void
test_18(QPDF& pdf, char const* arg2)
{
    // Remove a page and re-insert it in the same file.
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();

    // Remove pages from various places, checking to make sure
    // that our pages reference is getting updated.
    assert(pages.size() == 10);
    QPDFObjectHandle page5 = pages.at(5);
    pdf.removePage(page5);
    assert(pages.size() == 9);
    pdf.addPage(page5, false);
    assert(pages.size() == 10);
    assert(pages.back().getObjGen() == page5.getObjGen());

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_19(QPDF& pdf, char const* arg2)
{
    // Remove a page and re-insert it in the same file.
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();

    // Try to insert a page that's already there. A shallow copy
    // gets inserted instead.
    auto newpage = pages.at(5);
    size_t count = pages.size();
    pdf.addPage(newpage, false);
    auto last = pages.back();
    assert(pages.size() == count + 1);
    assert(!(last.getObjGen() == newpage.getObjGen()));
    assert(
        last.getKey("/Contents").getObjGen() ==
        newpage.getKey("/Contents").getObjGen());
}

static void
test_20(QPDF& pdf, char const* arg2)
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

static void
test_21(QPDF& pdf, char const* arg2)
{
    // Try to shallow copy a stream
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
    QPDFObjectHandle page = pages.at(0);
    QPDFObjectHandle contents = page.getKey("/Contents");
    contents.shallowCopy();
    std::cout << "you can't see this" << std::endl;
}

static void
test_22(QPDF& pdf, char const* arg2)
{
    // Try to remove a page we don't have
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    QPDFPageObjectHelper& page = pages.at(0);
    dh.removePage(page);
    dh.removePage(page);
    std::cout << "you can't see this" << std::endl;
}

static void
test_23(QPDF& pdf, char const* arg2)
{
    QPDFPageDocumentHelper dh(pdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    dh.removePage(pages.back());
}

static void
test_24(QPDF& pdf, char const* arg2)
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
    if (res1.isArray()) {
        std::cout << "oops -- res1 is an array" << std::endl;
    }
    if (res1.isReserved()) {
        std::cout << "res1 is still reserved after checking if array"
                  << std::endl;
    }
    pdf.replaceReserved(res1, array1);
    if (res1.isReserved()) {
        std::cout << "oops -- res1 is still reserved" << std::endl;
    } else {
        std::cout << "res1 is no longer reserved" << std::endl;
    }
    res1.assertArray();
    std::cout << "res1 is an array" << std::endl;

    try {
        res2.unparseResolved();
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }
    try {
        res2.makeDirect();
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }

    pdf.replaceReserved(res2, array2);

    res2.assertArray();
    std::cout << "res2 is an array" << std::endl;

    // Verify that the previously added reserved keys can be
    // dereferenced properly now
    int i1 = res1.getArrayItem(0).getArrayItem(1).getIntValueAsInt();
    int i2 = res2.getArrayItem(0).getArrayItem(1).getIntValueAsInt();
    if ((i1 == 2) && (i2 == 1)) {
        std::cout << "circular access and lazy resolution worked" << std::endl;
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_25(QPDF& pdf, char const* arg2)
{
    // The copy object tests are designed to work with a specific
    // file.  Look at the test suite for the file, and look at the
    // file for comments about the file's structure.

    // Copy qtest without crossing page boundaries.  Should get O1
    // and O2 and their streams but not O3 or any other pages.

    assert(arg2 != 0);
    {
        // Make sure original PDF is out of scope when we write.
        QPDF oldpdf;
        oldpdf.processFile(arg2);
        QPDFObjectHandle qtest = oldpdf.getTrailer().getKey("/QTest");
        pdf.getTrailer().replaceKey("/QTest", pdf.copyForeignObject(qtest));
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_26(QPDF& pdf, char const* arg2)
{
    // Copy the O3 page using addPage.  Copy qtest without
    // crossing page boundaries.  In addition to previous results,
    // should get page O3 but no other pages including the page
    // that O3 points to.  Also, inherited object will have been
    // pushed down and will be preserved.

    {
        // Make sure original PDF is out of scope when we write.
        assert(arg2 != 0);
        QPDF oldpdf;
        oldpdf.processFile(arg2);
        QPDFObjectHandle qtest = oldpdf.getTrailer().getKey("/QTest");
        QPDFObjectHandle O3 = qtest.getKey("/O3");
        QPDFPageDocumentHelper(pdf).addPage(O3, false);
        pdf.getTrailer().replaceKey("/QTest", pdf.copyForeignObject(qtest));
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setStreamDataMode(qpdf_s_preserve);
    w.write();
}

static void
test_27(QPDF& pdf, char const* arg2)
{
    // Copy O3 and the page O3 refers to before copying qtest.
    // Should get qtest plus only the O3 page and the page that O3
    // points to. Inherited objects should be preserved. This test
    // also exercises copying from a stream that has a buffer and
    // a provider, including copying a provider multiple times. We
    // also exercise setImmediateCopyFrom.

    // Create a provider. The provider stays in scope.
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> p1;
    {
        // Local scope
        Pl_Buffer pl("buffer");
        pl.writeCStr("new data for stream\n");
        pl.finish();
        auto b = pl.getBufferSharedPointer();
        Provider* provider = new Provider(b);
        p1 = decltype(p1)(provider);
    }
    // Create a stream that uses a provider in empty1 and copy it
    // to empty2. It is copied from empty2 to the final pdf.
    QPDF empty1;
    empty1.emptyPDF();
    QPDFObjectHandle s1 = QPDFObjectHandle::newStream(&empty1);
    s1.replaceStreamData(
        p1, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    QPDF empty2;
    empty2.emptyPDF();
    s1 = empty2.copyForeignObject(s1);
    {
        // Make sure some source PDFs are out of scope when we
        // write.

        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> p2;
        // Create another provider. This one will go out of scope
        // along with its containing qpdf, which has
        // setImmediateCopyFrom(true).
        {
            // Local scope
            Pl_Buffer pl("buffer");
            pl.writeCStr("more data for stream\n");
            pl.finish();
            auto b = pl.getBufferSharedPointer();
            Provider* provider = new Provider(b);
            p2 = decltype(p2)(provider);
        }
        QPDF empty3;
        empty3.emptyPDF();
        empty3.setImmediateCopyFrom(true);
        QPDFObjectHandle s3 = QPDFObjectHandle::newStream(&empty3);
        s3.replaceStreamData(
            p2, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        assert(arg2 != 0);
        QPDF oldpdf;
        oldpdf.processFile(arg2);
        QPDFObjectHandle qtest = oldpdf.getTrailer().getKey("/QTest");
        QPDFObjectHandle O3 = qtest.getKey("/O3");
        QPDFPageDocumentHelper dh(pdf);
        dh.addPage(O3.getKey("/OtherPage"), false);
        dh.addPage(O3, false);
        QPDFObjectHandle s2 = QPDFObjectHandle::newStream(&oldpdf, "potato\n");
        auto trailer = pdf.getTrailer();
        trailer.replaceKey("/QTest", pdf.copyForeignObject(qtest));
        auto qtest2 = trailer.replaceKeyAndGetNew(
            "/QTest2", QPDFObjectHandle::newArray());
        qtest2.appendItem(pdf.copyForeignObject(s1));
        qtest2.appendItem(pdf.copyForeignObject(s2));
        qtest2.appendItem(pdf.copyForeignObject(s3));
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setCompressStreams(false);
    w.setDecodeLevel(qpdf_dl_generalized);
    w.write();
}

static void
test_28(QPDF& pdf, char const* arg2)
{
    // Copy foreign object errors
    try {
        pdf.copyForeignObject(pdf.getTrailer().getKey("/QTest"));
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }
    try {
        pdf.copyForeignObject(QPDFObjectHandle::newInteger(1));
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }
}

static void
test_29(QPDF& pdf, char const* arg2)
{
    // Detect mixed objects in QPDFWriter
    assert(arg2 != 0);
    auto other = QPDF::create();
    other->processFile(arg2);
    // We need to create a QPDF with mixed ownership to exercise
    // QPDFWriter's ownership check. To do this, we have to sneak the
    // foreign object inside an ownerless direct object to avoid
    // detection prior to calling QPDFWriter. Maybe a future version
    // of qpdf will be able prevent creating mixed ownership. Another
    // way to fake it out would be to call setDescription to
    // explicitly change the ownership to the wrong value.
    auto dict = QPDFObjectHandle::newDictionary();
    dict.replaceKey("/QTest", pdf.getTrailer().getKey("/QTest"));
    other->getTrailer().replaceKey("/QTest", dict);

    try {
        QPDFWriter w(*other, "a.pdf");
        w.write();
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }

    // Make sure deleting the other source doesn't prevent detection.
    auto other2 = QPDF::create();
    other2->emptyPDF();
    dict = QPDFObjectHandle::newDictionary();
    dict.replaceKey("/QTest", other2->getRoot());
    other->getTrailer().replaceKey("/QTest", dict);
    other2 = nullptr;
    try {
        QPDFWriter w(*other, "a.pdf");
        w.write();
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }

    // Detect adding a foreign object
    auto root1 = pdf.getRoot();
    auto root2 = other->getRoot();
    try {
        root1.replaceKey("/Oops", root2);
    } catch (std::logic_error const& e) {
        std::cout << "logic error: " << e.what() << std::endl;
    }
}

static void
test_30(QPDF& pdf, char const* arg2)
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
    if (orig_contents != new_contents) {
        std::cout << "oops -- page contents don't match" << std::endl
                  << "original:\n"
                  << orig_contents << "new:\n"
                  << new_contents << std::endl;
    }
}

static void
test_31(QPDF& pdf, char const* arg2)
{
    // Test object parsing from a string.  The input file is not used.

    auto o1 = "[/name 16059 3.14159 false\n"
              " << /key true /other [ (string1) (string2) ] >> null]"_qpdf;
    std::cout << o1.unparse() << std::endl;
    QPDFObjectHandle o2 = QPDFObjectHandle::parse("   12345 \f  ");
    assert(o2.isInteger() && (o2.getIntValue() == 12345));
    try {
        QPDFObjectHandle::parse("[1 0 R]", "indirect test");
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::logic_error const& e) {
        std::cout << "logic error parsing indirect: " << e.what() << std::endl;
    }
    try {
        QPDFObjectHandle::parse("0 trailing", "trailing test");
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::runtime_error const& e) {
        std::cout << "trailing data: " << e.what() << std::endl;
    }
    assert(
        QPDFObjectHandle::parse(&pdf, "[1 0 R]", "indirect test").unparse() ==
        "[ 1 0 R ]");
}

static void
test_32(QPDF& pdf, char const* arg2)
{
    // Extra header text
    char const* filenames[] = {"a.pdf", "b.pdf", "c.pdf", "d.pdf"};
    for (int i = 0; i < 4; ++i) {
        bool linearized = ((i & 1) != 0);
        bool newline = ((i & 2) != 0);
        QPDFWriter w(pdf, filenames[i]);
        w.setStaticID(true);
        std::cout << "file: " << filenames[i] << std::endl
                  << "linearized: " << (linearized ? "yes" : "no") << std::endl
                  << "newline: " << (newline ? "yes" : "no") << std::endl;
        w.setLinearization(linearized);
        w.setExtraHeaderText(
            newline ? "%% Comment with newline\n" : "%% Comment\n% No newline");
        w.write();
    }
}

static void
test_33(QPDF& pdf, char const* arg2)
{
    // Test writing to a custom pipeline
    Pl_Buffer p("buffer");
    QPDFWriter w(pdf);
    w.setStaticID(true);
    w.setOutputPipeline(&p);
    w.write();
    auto b = p.getBufferSharedPointer();
    FILE* f = QUtil::safe_fopen("a.pdf", "wb");
    fwrite(b->getBuffer(), b->getSize(), 1, f);
    fclose(f);
}

static void
test_34(QPDF& pdf, char const* arg2)
{
    // Look at Extensions dictionary
    std::cout << "version: " << pdf.getPDFVersion() << std::endl
              << "extension level: " << pdf.getExtensionLevel() << std::endl
              << pdf.getRoot().getKey("/Extensions").unparse() << std::endl;
    auto v = pdf.getVersionAsPDFVersion();
    std::string v_string;
    int extension_level;
    v.getVersion(v_string, extension_level);
    std::cout << "As PDFVersion: " << v_string << "/" << extension_level
              << std::endl;
}

static void
test_35(QPDF& pdf, char const* arg2)
{
    // Extract attachments

    std::map<std::string, std::shared_ptr<Buffer>> attachments;
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle names = root.getKey("/Names");
    QPDFObjectHandle embeddedFiles = names.getKey("/EmbeddedFiles");
    names = embeddedFiles.getKey("/Names");
    for (int i = 0; i < names.getArrayNItems(); ++i) {
        QPDFObjectHandle item = names.getArrayItem(i);
        if (item.isDictionary() && item.getKey("/Type").isName() &&
            (item.getKey("/Type").getName() == "/Filespec") &&
            item.getKey("/EF").isDictionary() &&
            item.getKey("/EF").getKey("/F").isStream()) {
            std::string filename = item.getKey("/F").getStringValue();
            QPDFObjectHandle stream = item.getKey("/EF").getKey("/F");
            attachments[filename] = stream.getStreamData();
        }
    }
    for (auto const& iter: attachments) {
        std::string const& filename = iter.first;
        std::string data = std::string(
            reinterpret_cast<char const*>(iter.second->getBuffer()),
            iter.second->getSize());
        bool is_binary = false;
        for (size_t i = 0; i < data.size(); ++i) {
            if ((data.at(i) < 0) || (data.at(i) > 126)) {
                is_binary = true;
                break;
            }
        }
        if (is_binary) {
            std::string t;
            for (size_t i = 0; i < std::min(data.size(), QIntC::to_size(20));
                 ++i) {
                if ((data.at(i) >= 32) && (data.at(i) <= 126)) {
                    t += data.at(i);
                } else {
                    t += ".";
                }
            }
            t += " (" + QUtil::uint_to_string(data.size()) + " bytes)";
            data = t;
        }
        std::cout << filename << ":\n" << data << "--END--\n";
    }
}

static void
test_36(QPDF& pdf, char const* arg2)
{
    // Extract raw unfilterable attachment

    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle names = root.getKey("/Names");
    QPDFObjectHandle embeddedFiles = names.getKey("/EmbeddedFiles");
    names = embeddedFiles.getKey("/Names");
    for (int i = 0; i < names.getArrayNItems(); ++i) {
        QPDFObjectHandle item = names.getArrayItem(i);
        if (item.isDictionary() && item.getKey("/Type").isName() &&
            (item.getKey("/Type").getName() == "/Filespec") &&
            item.getKey("/EF").isDictionary() &&
            item.getKey("/EF").getKey("/F").isStream() &&
            (item.getKey("/F").getStringValue() == "attachment1.txt")) {
            std::string filename = item.getKey("/F").getStringValue();
            QPDFObjectHandle stream = item.getKey("/EF").getKey("/F");
            Pl_Buffer p1("buffer");
            Pl_Flate p2("compress", &p1, Pl_Flate::a_inflate);
            stream.pipeStreamData(&p2, 0, qpdf_dl_none);
            auto buf = p1.getBufferSharedPointer();
            std::string data = std::string(
                reinterpret_cast<char const*>(buf->getBuffer()),
                buf->getSize());
            std::cout << stream.getDict().unparse() << filename << ":\n"
                      << data << "--END--\n";
        }
    }
}

static void
test_37(QPDF& pdf, char const* arg2)
{
    // Parse content streams of all pages
    for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        ParserCallbacks cb;
        page.parseContents(&cb);
    }
}

static void
test_38(QPDF& pdf, char const* arg2)
{
    // Designed for override-compressed-object.pdf
    QPDFObjectHandle qtest = pdf.getRoot().getKey("/QTest");
    for (int i = 0; i < qtest.getArrayNItems(); ++i) {
        std::cout << qtest.getArrayItem(i).unparseResolved() << std::endl;
    }
}

static void
test_39(QPDF& pdf, char const* arg2)
{
    // Display image filter and color set for each image on each page
    int pageno = 0;
    for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        std::cout << "page " << ++pageno << std::endl;
        std::map<std::string, QPDFObjectHandle> images = page.getImages();
        for (auto& i_iter: images) {
            QPDFObjectHandle image_dict = i_iter.second.getDict();
            std::cout << "filter: "
                      << image_dict.getKey("/Filter").unparseResolved()
                      << ", color space: "
                      << image_dict.getKey("/ColorSpace").unparseResolved()
                      << std::endl;
        }
    }
}

static void
test_40(QPDF& pdf, char const* arg2)
{
    // Write PCLm. This requires specially crafted PDF files. This
    // feature was implemented by Sahil Arora
    // <sahilarora.535@gmail.com> as part of a Google Summer of
    // Code project in 2017.
    assert(arg2 != 0);
    QPDFWriter w(pdf, arg2);
    w.setPCLm(true);
    w.setStaticID(true);
    w.write();
}

static void
test_41(QPDF& pdf, char const* arg2)
{
    // Apply a token filter. This test case is crafted to work
    // with coalesce.pdf.
    for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        page.addContentTokenFilter(
            std::shared_ptr<QPDFObjectHandle::TokenFilter>(new TokenFilter()));
    }
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();
}

static void
test_42(QPDF& pdf, char const* arg2)
{
    // Access objects as wrong type. This test case is crafted to
    // work with object-types.pdf.
    QPDFObjectHandle qtest = pdf.getTrailer().getKey("/QTest");
    QPDFObjectHandle array = qtest.getKey("/Dictionary").getKey("/Key2");
    QPDFObjectHandle dictionary = qtest.getKey("/Dictionary");
    QPDFObjectHandle integer = qtest.getKey("/Integer");
    QPDFObjectHandle null = QPDFObjectHandle::newNull();
    assert(array.isArray());
    {
        // Exercise iterators directly
        auto ai = array.aitems();
        auto i = ai.begin();
        assert(i->getName() == "/Item0");
        auto& i_value = *i;
        --i;
        assert(i->getName() == "/Item0");
        ++i;
        ++i;
        ++i;
        assert(i == ai.end());
        ++i;
        assert(i == ai.end());
        assert(!i_value.isInitialized());
        --i;
        assert(i_value.getName() == "/Item2");
        assert(i->getName() == "/Item2");
    }
    assert(dictionary.isDictionary());
    {
        // Exercise iterators directly
        auto di = dictionary.ditems();
        auto i = di.begin();
        assert(i->first == "/Key1");
        auto& i_value = *i;
        assert(i->second.getName() == "/Value1");
        ++i;
        ++i;
        assert(i == di.end());
        assert(!i_value.second.isInitialized());
    }
    assert("" == qtest.getStringValue());
    array.getArrayItem(-1).assertNull();
    array.getArrayItem(16059).assertNull();
    integer.getArrayItem(0).assertNull();
    integer.appendItem(null);
    array.eraseItem(-1);
    array.eraseItem(16059);
    integer.eraseItem(0);
    integer.insertItem(0, null);
    integer.setArrayFromVector(std::vector<QPDFObjectHandle>());
    integer.setArrayItem(0, null);
    assert(0 == integer.getArrayNItems());
    assert(integer.getArrayAsVector().empty());
    assert(false == integer.getBoolValue());
    assert(integer.getDictAsMap().empty());
    assert(integer.getKeys().empty());
    assert(false == integer.hasKey("/Potato"));
    integer.removeKey("/Potato");
    integer.replaceKey("/Potato", null);
    integer.replaceKey("/Potato", QPDFObjectHandle::newInteger(1));
    null.getKeyIfDict("/Integer").getKeyIfDict("/Potato").assertNull();
    qtest.getKey("/Integer").getKeyIfDict("/Potato");
    qtest.getKey("/Integer").getKey("/Potato");
    assert(integer.getInlineImageValue().empty());
    assert(0 == dictionary.getIntValue());
    assert("/QPDFFakeName" == integer.getName());
    assert("QPDFFAKE" == integer.getOperatorValue());
    assert("0.0" == dictionary.getRealValue());
    assert(integer.getStringValue().empty());
    assert(integer.getUTF8Value().empty());
    assert(0.0 == dictionary.getNumericValue());
    // Make sure error messages are okay for nested values
    std::cerr << "One error\n";
    assert(array.getArrayItem(0).getStringValue().empty());
    std::cerr << "One error\n";
    assert(dictionary.getKey("/Quack").getStringValue().empty());
    assert(dictionary.getKeyIfDict("/Quack").getStringValue().empty());
    assert(array.getArrayItem(1).isDictionary());
    assert(array.getArrayItem(1).getKey("/K").isArray());
    assert(array.getArrayItem(1).getKey("/K").getArrayItem(0).isName());
    assert(
        "/V" == array.getArrayItem(1).getKey("/K").getArrayItem(0).getName());
    std::cerr << "Two errors\n";
    assert(array.getArrayItem(16059).getStringValue().empty());
    std::cerr << "One error\n";
    array.getArrayItem(1).getKey("/K").getArrayItem(0).getStringValue();
    // Stream dictionary
    QPDFObjectHandle page = pdf.getAllPages().at(0);
    assert(
        "/QPDFFakeName" ==
        page.getKey("/Contents").getDict().getKey("/Potato").getName());
    // Rectangles
    QPDFObjectHandle::Rectangle r0 = integer.getArrayAsRectangle();
    assert((r0.llx == 0) && (r0.lly == 0) && (r0.urx == 0) && (r0.ury == 0));
    QPDFObjectHandle rect = QPDFObjectHandle::newFromRectangle(
        QPDFObjectHandle::Rectangle(1.2, 3.4, 5.6, 7.8));
    QPDFObjectHandle::Rectangle r1 = rect.getArrayAsRectangle();
    assert(
        (r1.llx > 1.19) && (r1.llx < 1.21) && (r1.lly > 3.39) &&
        (r1.lly < 3.41) && (r1.urx > 5.59) && (r1.urx < 5.61) &&
        (r1.ury > 7.79) && (r1.ury < 7.81));
    QPDFObjectHandle uninitialized;
    assert(!uninitialized.isInitialized());
    assert(!uninitialized.isInteger());
    assert(!uninitialized.isDictionary());
}

static void
test_43(QPDF& pdf, char const* arg2)
{
    // Forms
    QPDFAcroFormDocumentHelper afdh(pdf);
    if (!afdh.hasAcroForm()) {
        std::cout << "no forms\n";
        return;
    }
    std::cout << "iterating over form fields\n";
    for (auto& ffh: afdh.getFormFields()) {
        std::cout << "Field: " << ffh.getObjectHandle().unparse() << std::endl;
        QPDFFormFieldObjectHelper node = ffh;
        while (!node.isNull()) {
            QPDFFormFieldObjectHelper parent(node.getParent());
            std::cout << "  Parent: "
                      << (parent.isNull() ? std::string("none")
                                          : parent.getObjectHandle().unparse())
                      << std::endl;
            node = parent;
        }
        std::cout << "  Fully qualified name: " << ffh.getFullyQualifiedName()
                  << std::endl;
        std::cout << "  Partial name: " << ffh.getPartialName() << std::endl;
        std::cout << "  Alternative name: " << ffh.getAlternativeName()
                  << std::endl;
        std::cout << "  Mapping name: " << ffh.getMappingName() << std::endl;
        std::cout << "  Field type: " << ffh.getFieldType() << std::endl;
        std::cout << "  Value: " << ffh.getValue().unparse() << std::endl;
        std::cout << "  Value as string: " << ffh.getValueAsString()
                  << std::endl;
        std::cout << "  Default value: " << ffh.getDefaultValue().unparse()
                  << std::endl;
        std::cout << "  Default value as string: "
                  << ffh.getDefaultValueAsString() << std::endl;
        std::cout << "  Default appearance: " << ffh.getDefaultAppearance()
                  << std::endl;
        std::cout << "  Quadding: " << ffh.getQuadding() << std::endl;
        std::vector<QPDFAnnotationObjectHelper> annotations =
            afdh.getAnnotationsForField(ffh);
        for (auto& aoh: annotations) {
            std::cout << "  Annotation: " << aoh.getObjectHandle().unparse()
                      << std::endl;
        }
    }
    std::cout << "iterating over annotations per page\n";
    for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        std::cout << "Page: " << page.getObjectHandle().unparse() << std::endl;
        for (auto& ah: afdh.getWidgetAnnotationsForPage(page)) {
            std::cout << "  Annotation: " << ah.getObjectHandle().unparse()
                      << std::endl;
            std::cout
                << "    Field: "
                << (afdh.getFieldForAnnotation(ah).getObjectHandle().unparse())
                << std::endl;
            std::cout << "    Subtype: " << ah.getSubtype() << std::endl;
            std::cout << "    Rect: ";
            print_rect(std::cout, ah.getRect());
            std::cout << std::endl;
            std::string state = ah.getAppearanceState();
            if (!state.empty()) {
                std::cout << "    Appearance state: " << state << std::endl;
            }
            std::cout << "    Appearance stream (/N): "
                      << ah.getAppearanceStream("/N").unparse() << std::endl;
            std::cout << "    Appearance stream (/N, /3): "
                      << ah.getAppearanceStream("/N", "/3").unparse()
                      << std::endl;
        }
    }
}

static void
test_44(QPDF& pdf, char const* arg2)
{
    // Set form fields.
    for (auto& field: QPDFAcroFormDocumentHelper(pdf).getFormFields()) {
        QPDFObjectHandle ft = field.getInheritableFieldValue("/FT");
        if (ft.isName() && (ft.getName() == "/Tx")) {
            // \xc3\xb7 is utf-8 for U+00F7 (divided by)
            field.setV("3.14 \xc3\xb7 0");
            std::cout << "Set field value: " << field.getFullyQualifiedName()
                      << " -> " << field.getValueAsString() << std::endl;
        }
    }
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.setSuppressOriginalObjectIDs(true);
    w.write();
}

static void
test_45(QPDF& pdf, char const* arg2)
{
    // Decode obfuscated files. This is here to help test with
    // files that trigger anti-virus warnings. See comments in
    // qpdf.test for details.
    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.write();
    if (!pdf.getWarnings().empty()) {
        exit(3);
    }
}

static void
test_46(QPDF& pdf, char const* arg2)
{
    // Test number tree. This test is crafted to work with
    // number-tree.pdf
    QPDFObjectHandle qtest = pdf.getTrailer().getKey("/QTest");
    QPDFNumberTreeObjectHelper ntoh(qtest, pdf);
    for (auto& iter: ntoh) {
        std::cout << iter.first << " " << iter.second.getStringValue()
                  << std::endl;
    }
    QPDFNumberTreeObjectHelper::idx_map ntoh_map = ntoh.getAsMap();
    for (auto& iter: ntoh_map) {
        std::cout << iter.first << " " << iter.second.getStringValue()
                  << std::endl;
    }
    assert(1 == ntoh.getMin());
    assert(29 == ntoh.getMax());
    assert(ntoh.hasIndex(6));
    assert(!ntoh.hasIndex(500));
    QPDFObjectHandle oh;
    assert(!ntoh.findObject(4, oh));
    assert(ntoh.findObject(3, oh));
    assert("three" == oh.getStringValue());
    QPDFNumberTreeObjectHelper::numtree_number offset = 0;
    assert(!ntoh.findObjectAtOrBelow(0, oh, offset));
    assert(ntoh.findObjectAtOrBelow(8, oh, offset));
    assert("six" == oh.getStringValue());
    assert(2 == offset);

    auto new1 = QPDFNumberTreeObjectHelper::newEmpty(pdf);
    auto iter1 = new1.begin();
    assert(iter1 == new1.end());
    ++iter1;
    assert(iter1 == new1.end());
    --iter1;
    assert(iter1 == new1.end());
    new1.insert(1, QPDFObjectHandle::newString("1"));
    ++iter1;
    assert((*iter1).first == 1); // exercise operator* explicitly
    auto& iter1_val = *iter1;
    --iter1;
    assert(iter1 == new1.end());
    --iter1;
    assert(iter1->first == 1);
    assert(iter1_val.first == 1);
    new1.insert(2, QPDFObjectHandle::newString("2"));
    ++iter1;
    assert(iter1->first == 2);
    assert(iter1_val.first == 2);
    ++iter1;
    assert(iter1 == new1.end());
    assert(!iter1_val.second.isInitialized());
    ++iter1;
    assert(iter1->first == 1);
    --iter1;
    assert(iter1 == new1.end());
    --iter1;
    assert(iter1->first == 2);

    std::cout << "insertAfter" << std::endl;
    auto new2 = QPDFNumberTreeObjectHelper::newEmpty(pdf);
    auto iter2 = new2.begin();
    assert(iter2 == new2.end());
    iter2.insertAfter(3, QPDFObjectHandle::newString("3!"));
    assert(iter2->first == 3);
    iter2.insertAfter(4, QPDFObjectHandle::newString("4!"));
    assert(iter2->first == 4);
    for (auto& i: new2) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }

    std::cout << "/Bad1" << std::endl;
    auto bad1 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Bad1"), pdf);
    assert(bad1.begin() == bad1.end());
    assert(bad1.last() == bad1.end());

    std::cout << "/Bad2" << std::endl;
    auto bad2 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Bad2"), pdf);
    for (auto& i: bad2) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }

    std::vector<std::string> empties = {"/Empty1", "/Empty2"};
    for (auto const& k: empties) {
        std::cout << k << std::endl;
        auto empty =
            QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey(k), pdf);
        assert(empty.begin() == empty.end());
        assert(empty.last() == empty.end());
        auto i = empty.insert(5, QPDFObjectHandle::newString("5"));
        assert(i->first == 5);
        assert(i->second.getStringValue() == "5");
        assert(empty.begin()->first == 5);
        assert(empty.last()->first == 5);
        assert(empty.begin()->second.getStringValue() == "5");
        i = empty.insert(5, QPDFObjectHandle::newString("5+"));
        assert(i->first == 5);
        assert(i->second.getStringValue() == "5+");
        assert(empty.begin()->second.getStringValue() == "5+");
        i = empty.insert(6, QPDFObjectHandle::newString("6"));
        assert(i->first == 6);
        assert(i->second.getStringValue() == "6");
        assert(empty.begin()->second.getStringValue() == "5+");
        assert(empty.last()->first == 6);
        assert(empty.last()->second.getStringValue() == "6");
    }
    std::cout << "Insert into invalid" << std::endl;
    auto invalid1 =
        QPDFNumberTreeObjectHelper(QPDFObjectHandle::newDictionary(), pdf);
    try {
        invalid1.insert(1, QPDFObjectHandle::newNull());
    } catch (QPDFExc& e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "/Bad3, no repair" << std::endl;
    auto bad3_oh = pdf.getTrailer().getKey("/Bad3");
    auto bad3 = QPDFNumberTreeObjectHelper(bad3_oh, pdf, false);
    for (auto& i: bad3) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }
    assert(!bad3_oh.getKey("/Kids").getArrayItem(0).isIndirect());

    std::cout << "/Bad3, repair" << std::endl;
    bad3 = QPDFNumberTreeObjectHelper(bad3_oh, pdf, true);
    for (auto& i: bad3) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }
    assert(bad3_oh.getKey("/Kids").getArrayItem(0).isIndirect());

    std::cout << "/Bad4 -- missing limits" << std::endl;
    auto bad4 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Bad4"), pdf);
    bad4.insert(5, QPDFObjectHandle::newString("5"));
    for (auto& i: bad4) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }

    std::cout << "/Bad5 -- limit errors" << std::endl;
    auto bad5 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Bad5"), pdf);
    assert(bad5.find(10) == bad5.end());
}

static void
test_47(QPDF& pdf, char const* arg2)
{
    // Test page labels.
    QPDFPageLabelDocumentHelper pldh(pdf);
    long long npages =
        pdf.getRoot().getKey("/Pages").getKey("/Count").getIntValue();
    std::vector<QPDFObjectHandle> labels;
    pldh.getLabelsForPageRange(0, npages - 1, 1, labels);
    assert(labels.size() % 2 == 0);
    for (size_t i = 0; i < labels.size(); i += 2) {
        std::cout << labels.at(i).getIntValue() << " "
                  << labels.at(i + 1).unparse() << std::endl;
    }
}

static void
test_48(QPDF& pdf, char const* arg2)
{
    // Test name tree. This test is crafted to work with
    // name-tree.pdf
    QPDFObjectHandle qtest = pdf.getTrailer().getKey("/QTest");
    QPDFNameTreeObjectHelper ntoh(qtest, pdf);
    for (auto& iter: ntoh) {
        std::cout << iter.first << " -> " << iter.second.getStringValue()
                  << std::endl;
    }
    std::map<std::string, QPDFObjectHandle> ntoh_map = ntoh.getAsMap();
    for (auto& iter: ntoh_map) {
        std::cout << iter.first << " -> " << iter.second.getStringValue()
                  << std::endl;
    }
    assert(ntoh.hasName("11 elephant"));
    assert(ntoh.hasName("07 sev\xe2\x80\xa2n"));
    assert(!ntoh.hasName("potato"));
    QPDFObjectHandle oh;
    assert(!ntoh.findObject("potato", oh));
    assert(ntoh.findObject("07 sev\xe2\x80\xa2n", oh));
    assert("seven!" == oh.getStringValue());
    auto last = ntoh.last();
    assert(last->first == "29 twenty-nine");
    assert(last->second.getUTF8Value() == "twenty-nine!");

    auto new1 = QPDFNameTreeObjectHelper::newEmpty(pdf);
    auto iter1 = new1.begin();
    assert(iter1 == new1.end());
    ++iter1;
    assert(iter1 == new1.end());
    --iter1;
    assert(iter1 == new1.end());
    new1.insert("1", QPDFObjectHandle::newString("1"));
    ++iter1;
    assert(iter1->first == "1");
    auto& iter1_val = *iter1;
    --iter1;
    assert(iter1 == new1.end());
    --iter1;
    assert(iter1->first == "1");
    assert(iter1_val.first == "1");
    new1.insert("2", QPDFObjectHandle::newString("2"));
    ++iter1;
    assert(iter1->first == "2");
    assert(iter1_val.first == "2");
    ++iter1;
    assert(iter1 == new1.end());
    assert(!iter1_val.second.isInitialized());
    ++iter1;
    assert(iter1->first == "1");
    --iter1;
    assert(iter1 == new1.end());
    --iter1;
    assert(iter1->first == "2");

    std::cout << "insertAfter" << std::endl;
    auto new2 = QPDFNameTreeObjectHelper::newEmpty(pdf);
    auto iter2 = new2.begin();
    assert(iter2 == new2.end());
    iter2.insertAfter("3", QPDFObjectHandle::newString("3!"));
    assert(iter2->first == "3");
    iter2.insertAfter("4", QPDFObjectHandle::newString("4!"));
    assert(iter2->first == "4");
    for (auto& i: new2) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }

    std::vector<std::string> empties = {"/Empty1", "/Empty2"};
    for (auto const& k: empties) {
        std::cout << k << std::endl;
        auto empty = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey(k), pdf);
        assert(empty.begin() == empty.end());
        assert(empty.last() == empty.end());
        auto i = empty.insert("five", QPDFObjectHandle::newString("5"));
        assert(i->first == "five");
        assert(i->second.getStringValue() == "5");
        assert(empty.begin()->first == "five");
        assert(empty.last()->first == "five");
        assert(empty.begin()->second.getStringValue() == "5");
        i = empty.insert("five", QPDFObjectHandle::newString("5+"));
        assert(i->first == "five");
        assert(i->second.getStringValue() == "5+");
        assert(empty.begin()->second.getStringValue() == "5+");
        i = empty.insert("six", QPDFObjectHandle::newString("6"));
        assert(i->first == "six");
        assert(i->second.getStringValue() == "6");
        assert(empty.begin()->second.getStringValue() == "5+");
        assert(empty.last()->first == "six");
        assert(empty.last()->second.getStringValue() == "6");
    }

    std::cout << "/Bad1 -- wrong key type" << std::endl;
    auto bad1 = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Bad1"), pdf);
    assert(bad1.find("G", true)->first == "A");
    for (auto const& i: bad1) {
        std::cout << i.first << std::endl;
    }

    std::cout << "/Bad2 -- invalid kid" << std::endl;
    auto bad2 = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Bad2"), pdf);
    assert(bad2.find("G", true)->first == "B");
    for (auto const& i: bad2) {
        std::cout << i.first << std::endl;
    }

    std::cout << "/Bad3 -- invalid kid" << std::endl;
    auto bad3 = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Bad3"), pdf);
    assert(bad3.find("G", true) == bad3.end());

    std::cout << "/Bad4 -- invalid kid" << std::endl;
    auto bad4 = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Bad4"), pdf);
    assert(bad4.find("F", true)->first == "C");
    for (auto const& i: bad4) {
        std::cout << i.first << std::endl;
    }

    std::cout << "/Bad5 -- loop in find" << std::endl;
    auto bad5 = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Bad5"), pdf);
    assert(bad5.find("F", true)->first == "D");

    std::cout << "/Bad6 -- bad limits" << std::endl;
    auto bad6 = QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Bad6"), pdf);
    assert(bad6.insert("H", QPDFObjectHandle::newNull())->first == "H");
}

static void
test_49(QPDF& pdf, char const* arg2)
{
    // Outlines
    QPDFOutlineDocumentHelper odh(pdf);
    int pageno = 0;
    for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
        auto outlines =
            odh.getOutlinesForPage(page.getObjectHandle().getObjGen());
        for (auto& ol: outlines) {
            std::cout << "page " << pageno << ": " << ol.getTitle() << " -> "
                      << ol.getDest().unparseResolved() << std::endl;
        }
        ++pageno;
    }
}

static void
test_50(QPDF& pdf, char const* arg2)
{
    // Test dictionary merge. This test is crafted to work with
    // merge-dict.pdf
    QPDFObjectHandle d1 = pdf.getTrailer().getKey("/Dict1");
    QPDFObjectHandle d2 = pdf.getTrailer().getKey("/Dict2");
    d1.mergeResources(d2);
    std::cout << d1.getJSON(JSON::LATEST).unparse() << std::endl;
    // Top-level type mismatch
    d1.mergeResources(d2.getKey("/k1"));
    for (auto const& name: d1.getResourceNames()) {
        std::cout << name << std::endl;
    }
}

static void
test_51(QPDF& pdf, char const* arg2)
{
    // Test radio button and checkbox field setting. The input
    // files must have radios button called r1 and r2 and
    // checkboxes called checkbox1 and checkbox2. The files
    // button-set*.pdf are designed for this test case.
    QPDFObjectHandle acroform = pdf.getRoot().getKey("/AcroForm");
    QPDFObjectHandle fields = acroform.getKey("/Fields");
    int nitems = fields.getArrayNItems();
    for (int i = 0; i < nitems; ++i) {
        QPDFObjectHandle field = fields.getArrayItem(i);
        QPDFObjectHandle T = field.getKey("/T");
        if (!T.isString()) {
            continue;
        }
        std::string Tval = T.getUTF8Value();
        if (Tval == "r1") {
            std::cout << "setting r1 via parent\n";
            QPDFFormFieldObjectHelper foh(field);
            foh.setV(QPDFObjectHandle::newName("/2"));
        } else if (Tval == "r2") {
            std::cout << "setting r2 via child\n";
            field = field.getKey("/Kids").getArrayItem(1);
            QPDFFormFieldObjectHelper foh(field);
            foh.setV(QPDFObjectHandle::newName("/3"));
        } else if (Tval == "checkbox1") {
            std::cout << "turning checkbox1 on\n";
            QPDFFormFieldObjectHelper foh(field);
            foh.setV(QPDFObjectHandle::newName("/Yes"));
        } else if (Tval == "checkbox2") {
            std::cout << "turning checkbox2 off\n";
            QPDFFormFieldObjectHelper foh(field);
            foh.setV(QPDFObjectHandle::newName("/Off"));
        }
    }
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();
}

static void
test_52(QPDF& pdf, char const* arg2)
{
    // This test just sets a field value for appearance stream
    // generating testing.
    QPDFObjectHandle acroform = pdf.getRoot().getKey("/AcroForm");
    QPDFObjectHandle fields = acroform.getKey("/Fields");
    int nitems = fields.getArrayNItems();
    for (int i = 0; i < nitems; ++i) {
        QPDFObjectHandle field = fields.getArrayItem(i);
        QPDFObjectHandle T = field.getKey("/T");
        if (!T.isString()) {
            continue;
        }
        std::string Tval = T.getUTF8Value();
        if (Tval == "list1") {
            std::cout << "setting list1 value\n";
            QPDFFormFieldObjectHelper foh(field);
            foh.setV(QPDFObjectHandle::newString(arg2));
        }
    }
    QPDFWriter w(pdf, "a.pdf");
    w.write();
}

static void
test_53(QPDF& pdf, char const* arg2)
{
    // Test get all objects and dangling ref handling
    QPDFObjectHandle root = pdf.getRoot();
    root.replaceKey(
        "/Q1", pdf.makeIndirectObject(QPDFObjectHandle::newString("potato")));
    std::cout << "all objects" << std::endl;
    for (auto& obj: pdf.getAllObjects()) {
        std::cout << obj.unparse() << std::endl;
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.write();
}

static void
test_54(QPDF& pdf, char const* arg2)
{
    // Test getFinalVersion. This must be invoked with a file
    // whose final version is not 1.5.
    QPDFWriter w(pdf, "a.pdf");
    assert(pdf.getPDFVersion() != "1.5");
    w.setObjectStreamMode(qpdf_o_generate);
    if (w.getFinalVersion() != "1.5") {
        std::cout << "oops: " << w.getFinalVersion() << std::endl;
    }
}

static void
test_55(QPDF& pdf, char const* arg2)
{
    // Form XObjects
    std::vector<QPDFPageObjectHelper> pages =
        QPDFPageDocumentHelper(pdf).getAllPages();
    QPDFObjectHandle qtest = QPDFObjectHandle::newArray();
    for (auto& ph: pages) {
        qtest.appendItem(ph.getFormXObjectForPage());
        qtest.appendItem(ph.getFormXObjectForPage(false));
    }
    pdf.getTrailer().replaceKey("/QTest", qtest);
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();
}

static void
test_56_59(
    QPDF& pdf,
    char const* arg2,
    bool handle_from_transformation,
    bool invert_to_transformation)
{
    // red pages are from pdf, blue pages are from pdf2
    // red pages always have stated rotation absolutely
    // 56: blue pages are overlaid exactly on top of red pages
    // 57: blue pages have stated rotation relative to red pages
    // 58: blue pages have no rotation (absolutely upright)
    // 59: blue pages have stated rotation absolutely

    // Placing form XObjects
    assert(arg2);
    QPDF pdf2;
    pdf2.processFile(arg2);

    std::vector<QPDFPageObjectHelper> pages1 =
        QPDFPageDocumentHelper(pdf).getAllPages();
    std::vector<QPDFPageObjectHelper> pages2 =
        QPDFPageDocumentHelper(pdf2).getAllPages();
    size_t npages =
        (pages1.size() < pages2.size() ? pages1.size() : pages2.size());
    for (size_t i = 0; i < npages; ++i) {
        QPDFPageObjectHelper& ph1 = pages1.at(i);
        QPDFPageObjectHelper& ph2 = pages2.at(i);
        QPDFObjectHandle fo = pdf.copyForeignObject(
            ph2.getFormXObjectForPage(handle_from_transformation));
        int min_suffix = 1;
        QPDFObjectHandle resources = ph1.getAttribute("/Resources", true);
        std::string name = resources.getUniqueResourceName("/Fx", min_suffix);
        std::string content = ph1.placeFormXObject(
            fo,
            name,
            ph1.getTrimBox().getArrayAsRectangle(),
            invert_to_transformation);
        if (!content.empty()) {
            resources.mergeResources(
                QPDFObjectHandle::parse("<< /XObject << >> >>"));
            resources.getKey("/XObject").replaceKey(name, fo);
            ph1.addPageContents(QPDFObjectHandle::newStream(&pdf, "q\n"), true);
            ph1.addPageContents(
                QPDFObjectHandle::newStream(&pdf, "\nQ\n" + content), false);
        }
    }
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();
}

static void
test_56(QPDF& pdf, char const* arg2)
{
    test_56_59(pdf, arg2, false, false);
}

static void
test_57(QPDF& pdf, char const* arg2)
{
    test_56_59(pdf, arg2, true, false);
}

static void
test_58(QPDF& pdf, char const* arg2)
{
    test_56_59(pdf, arg2, false, true);
}

static void
test_59(QPDF& pdf, char const* arg2)
{
    test_56_59(pdf, arg2, true, true);
}

static void
test_60(QPDF& pdf, char const* arg2)
{
    // Boundary condition testing for getUniqueResourceName;
    // additional testing of mergeResources with conflict
    // detection
    QPDFObjectHandle r1 = QPDFObjectHandle::newDictionary();
    int min_suffix = 1;
    for (int i = 1; i < 3; ++i) {
        std::string name = r1.getUniqueResourceName("/Quack", min_suffix);
        r1.mergeResources(QPDFObjectHandle::parse("<< /Z << >> >>"));
        r1.getKey("/Z").replaceKey(name, QPDFObjectHandle::newString("moo"));
    }
    auto make_resource = [&](QPDFObjectHandle& dict,
                             std::string const& key,
                             std::string const& str) {
        auto o1 = QPDFObjectHandle::newArray();
        o1.appendItem(QPDFObjectHandle::newString(str));
        dict.replaceKey(key, pdf.makeIndirectObject(o1));
    };

    auto z = r1.getKey("/Z");
    r1.replaceKey("/Y", QPDFObjectHandle::newDictionary());
    auto y = r1.getKey("/Y");
    make_resource(z, "/F1", "r1.Z.F1");
    make_resource(z, "/F2", "r1.Z.F2");
    make_resource(y, "/F2", "r1.Y.F2");
    make_resource(y, "/F3", "r1.Y.F3");
    QPDFObjectHandle r2 = QPDFObjectHandle::parse("<< /Z << >> /Y << >> >>");
    z = r2.getKey("/Z");
    y = r2.getKey("/Y");
    make_resource(z, "/F2", "r2.Z.F2");
    make_resource(y, "/F3", "r2.Y.F3");
    make_resource(y, "/F4", "r2.Y.F4");
    // Add a direct object
    y.replaceKey("/F5", QPDFObjectHandle::newString("direct r2.Y.F5"));

    std::map<std::string, std::map<std::string, std::string>> conflicts;
    auto show_conflicts = [&](std::string const& msg) {
        std::cout << msg << std::endl;
        for (auto const& i1: conflicts) {
            std::cout << i1.first << ":" << std::endl;
            for (auto const& i2: i1.second) {
                std::cout << "  " << i2.first << " -> " << i2.second
                          << std::endl;
            }
        }
    };

    r1.mergeResources(r2, &conflicts);
    show_conflicts("first merge");
    auto r3 = r1.shallowCopy();
    // Merge again. The direct object gets recopied. Everything
    // else is the same.
    r1.mergeResources(r2, &conflicts);
    show_conflicts("second merge");

    // Make all resources in r2 direct. Then merge two more times.
    // We should get the one previously direct object copied one
    // time as an indirect object.
    r2.makeResourcesIndirect(pdf);
    r1.mergeResources(r2, &conflicts);
    show_conflicts("third merge");
    r1.mergeResources(r2, &conflicts);
    show_conflicts("fourth merge");

    // The only differences between /QTest and /QTest3 should be
    // the direct objects merged from r2.
    auto trailer = pdf.getTrailer();
    trailer.replaceKey("/QTest1", r1);
    trailer.replaceKey("/QTest2", r2);
    trailer.replaceKey("/QTest3", r3);
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();
}

static void
test_61(QPDF& pdf, char const* arg2)
{
    // Test to make sure type information is passed across shared
    // library boundaries. This includes exception handling, dynamic
    // cast, and subclassing.
    pdf.setAttemptRecovery(false);
    pdf.setSuppressWarnings(true);
    try {
        pdf.processMemoryFile("empty", "", 0);
    } catch (QPDFExc const&) {
        std::cout << "Caught QPDFExc as expected" << std::endl;
    }
    try {
        QUtil::safe_fopen("/does/not/exist", "r");
    } catch (QPDFSystemError const&) {
        std::cout << "Caught QPDFSystemError as expected" << std::endl;
    }
    try {
        QUtil::int_to_string_base(0, 12);
    } catch (std::logic_error const&) {
        std::cout << "Caught logic_error as expected" << std::endl;
    }
    try {
        QUtil::toUTF8(0xffffffff);
    } catch (std::runtime_error const&) {
        std::cout << "Caught runtime_error as expected" << std::endl;
    }

    // Spot check RTTI for dynamic cast. We intend to have pipelines
    // and input sources be testable, but adding comprehensive tests
    // for everything doesn't add value as it wouldn't catch
    // forgetting QPDF_DLL_CLASS on a new subclass.
    BufferInputSource b("x", "y");
    InputSource* is = &b;
    assert(dynamic_cast<BufferInputSource*>(is) != nullptr);
    Pl_Discard pd;
    Pipeline* p = &pd;
    assert(dynamic_cast<Pl_Discard*>(p) != nullptr);

    // For some reason, QPDFNameTreeObjectHelper's vtable seems to
    // like to not make it into the shared library with mingw. Try to
    // make sure this is really fixed.
    QPDFNameTreeObjectHelper* n =
        new ExtendNameTree(QPDFObjectHandle::newNull(), pdf);
    delete n;
}

static void
test_62(QPDF& pdf, char const* arg2)
{
    // Test int size checks. This test will fail if int and long
    // long are the same size.
    QPDFObjectHandle t = pdf.getTrailer();
    unsigned long long q1_l = 3ULL * QIntC::to_ulonglong(INT_MAX);
    long long q1 = QIntC::to_longlong(q1_l);
    long long q2_l = 3LL * QIntC::to_longlong(INT_MIN);
    long long q2 = QIntC::to_longlong(q2_l);
    unsigned int q3_i = UINT_MAX;
    long long q3 = QIntC::to_longlong(q3_i);
    t.replaceKey("/Q1", QPDFObjectHandle::newInteger(q1));
    t.replaceKey("/Q2", QPDFObjectHandle::newInteger(q2));
    t.replaceKey("/Q3", QPDFObjectHandle::newInteger(q3));
    assert_compare_numbers(q1, t.getKey("/Q1").getIntValue());
    assert_compare_numbers(q1_l, t.getKey("/Q1").getUIntValue());
    assert_compare_numbers(INT_MAX, t.getKey("/Q1").getIntValueAsInt());
    assert_compare_numbers(UINT_MAX, t.getKey("/Q1").getUIntValueAsUInt());
    assert_compare_numbers(q2_l, t.getKey("/Q2").getIntValue());
    assert_compare_numbers(0U, t.getKey("/Q2").getUIntValue());
    assert_compare_numbers(INT_MIN, t.getKey("/Q2").getIntValueAsInt());
    assert_compare_numbers(0U, t.getKey("/Q2").getUIntValueAsUInt());
    assert_compare_numbers(INT_MAX, t.getKey("/Q3").getIntValueAsInt());
    assert_compare_numbers(UINT_MAX, t.getKey("/Q3").getUIntValueAsUInt());
}

static void
test_63(QPDF& pdf, char const* arg2)
{
    QPDFWriter w(pdf);
    // Exercise setting encryption parameters before setting the
    // output filename. The previous bug does not happen if static
    // or deterministic ID is used because the filename is not
    // used as part of the input data for ID generation in those
    // cases.
    w.setR6EncryptionParameters(
        "u", "o", true, true, true, true, true, true, qpdf_r3p_full, true);
    w.setOutputFilename("a.pdf");
    w.write();
}

static void
test_64_67(QPDF& pdf, char const* arg2, bool allow_shrink, bool allow_expand)
{
    // Overlay file2 on file1.
    // 64: allow neither shrink nor shrink
    // 65: allow shrink but not expand
    // 66: allow expand but not shrink
    // 67: allow both shrink and expand

    // Placing form XObjects: expand, shrink
    assert(arg2);
    QPDF pdf2;
    pdf2.processFile(arg2);

    std::vector<QPDFPageObjectHelper> pages1 =
        QPDFPageDocumentHelper(pdf).getAllPages();
    std::vector<QPDFPageObjectHelper> pages2 =
        QPDFPageDocumentHelper(pdf2).getAllPages();
    size_t npages =
        (pages1.size() < pages2.size() ? pages1.size() : pages2.size());
    for (size_t i = 0; i < npages; ++i) {
        QPDFPageObjectHelper& ph1 = pages1.at(i);
        QPDFPageObjectHelper& ph2 = pages2.at(i);
        QPDFObjectHandle fo =
            pdf.copyForeignObject(ph2.getFormXObjectForPage());
        int min_suffix = 1;
        QPDFObjectHandle resources = ph1.getAttribute("/Resources", true);
        std::string name = resources.getUniqueResourceName("/Fx", min_suffix);
        std::string content = ph1.placeFormXObject(
            fo,
            name,
            ph1.getTrimBox().getArrayAsRectangle(),
            false,
            allow_shrink,
            allow_expand);
        if (!content.empty()) {
            resources.mergeResources(
                QPDFObjectHandle::parse("<< /XObject << >> >>"));
            resources.getKey("/XObject").replaceKey(name, fo);
            ph1.addPageContents(QPDFObjectHandle::newStream(&pdf, "q\n"), true);
            ph1.addPageContents(
                QPDFObjectHandle::newStream(&pdf, "\nQ\n" + content), false);
        }
    }
    QPDFWriter w(pdf, "a.pdf");
    w.setQDFMode(true);
    w.setStaticID(true);
    w.write();
}

static void
test_64(QPDF& pdf, char const* arg2)
{
    test_64_67(pdf, arg2, false, false);
}

static void
test_65(QPDF& pdf, char const* arg2)
{
    test_64_67(pdf, arg2, true, false);
}

static void
test_66(QPDF& pdf, char const* arg2)
{
    test_64_67(pdf, arg2, false, true);
}

static void
test_67(QPDF& pdf, char const* arg2)
{
    test_64_67(pdf, arg2, true, true);
}

static void
test_68(QPDF& pdf, char const* arg2)
{
    QPDFObjectHandle root = pdf.getRoot();
    QPDFObjectHandle qstream = root.getKey("/QStream");
    try {
        qstream.getStreamData();
        std::cout << "oops -- didn't throw" << std::endl;
    } catch (std::exception& e) {
        std::cout << "get unfilterable stream: " << e.what() << std::endl;
    }
    std::shared_ptr<Buffer> b1 = qstream.getStreamData(qpdf_dl_all);
    if ((b1->getSize() > 10) &&
        (memcmp(b1->getBuffer(), "wwwwwwwww", 9) == 0)) {
        std::cout << "filtered stream data okay" << std::endl;
    }
    std::shared_ptr<Buffer> b2 = qstream.getRawStreamData();
    if ((b2->getSize() > 10) &&
        (memcmp(
             b2->getBuffer(), "\xff\xd8\xff\xe0\x00\x10\x4a\x46\x49\x46", 10) ==
         0)) {
        std::cout << "raw stream data okay" << std::endl;
    }
}

static void
test_69(QPDF& pdf, char const* arg2)
{
    pdf.setImmediateCopyFrom(true);
    auto pages = pdf.getAllPages();
    for (size_t i = 0; i < pages.size(); ++i) {
        QPDF out;
        out.emptyPDF();
        out.addPage(pages.at(i), false);
        std::string outname =
            std::string("auto-") + QUtil::uint_to_string(i) + ".pdf";
        QPDFWriter w(out, outname.c_str());
        w.setStaticID(true);
        w.write();
    }
}

static void
test_70(QPDF& pdf, char const* arg2)
{
    auto trailer = pdf.getTrailer();
    trailer.getKey("/S1").setFilterOnWrite(false);
    trailer.getKey("/S2").setFilterOnWrite(false);
    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setDecodeLevel(qpdf_dl_specialized);
    w.write();
}

static void
test_71(QPDF& pdf, char const* arg2)
{
    auto show = [](QPDFObjectHandle& obj,
                   QPDFObjectHandle& xobj_dict,
                   std::string const& key) {
        std::cout << xobj_dict.unparse() << " -> " << key << " -> "
                  << obj.unparse() << std::endl;
    };
    auto page = QPDFPageDocumentHelper(pdf).getAllPages().at(0);
    std::cout << "--- recursive, all ---" << std::endl;
    page.forEachXObject(true, show);
    std::cout << "--- non-recursive, all ---" << std::endl;
    page.forEachXObject(false, show);
    std::cout << "--- recursive, images ---" << std::endl;
    page.forEachImage(true, show);
    std::cout << "--- non-recursive, images ---" << std::endl;
    page.forEachImage(false, show);
    std::cout << "--- recursive, form XObjects ---" << std::endl;
    page.forEachFormXObject(true, show);
    std::cout << "--- non-recursive, form XObjects ---" << std::endl;
    page.forEachFormXObject(false, show);
    auto fx1 = QPDFPageObjectHelper(page.getObjectHandle()
                                        .getKey("/Resources")
                                        .getKey("/XObject")
                                        .getKey("/Fx1"));
    std::cout << "--- recursive, all, from fx1 ---" << std::endl;
    fx1.forEachXObject(true, show);
    std::cout << "--- non-recursive, all, from fx1 ---" << std::endl;
    fx1.forEachXObject(false, show);
    std::cout << "--- get images, page ---" << std::endl;
    for (auto& i: page.getImages()) {
        std::cout << i.first << " -> " << i.second.unparse() << std::endl;
    }
    std::cout << "--- get images, fx ---" << std::endl;
    for (auto& i: fx1.getImages()) {
        std::cout << i.first << " -> " << i.second.unparse() << std::endl;
    }
    std::cout << "--- get form XObjects, page ---" << std::endl;
    for (auto& i: page.getFormXObjects()) {
        std::cout << i.first << " -> " << i.second.unparse() << std::endl;
    }
    std::cout << "--- get form XObjects, fx ---" << std::endl;
    for (auto& i: fx1.getFormXObjects()) {
        std::cout << i.first << " -> " << i.second.unparse() << std::endl;
    }
}

static void
test_72(QPDF& pdf, char const* arg2)
{
    // Call some QPDFPageObjectHelper methods on form XObjects.
    auto page = QPDFPageDocumentHelper(pdf).getAllPages().at(0);
    auto fx1 = QPDFPageObjectHelper(page.getObjectHandle()
                                        .getKey("/Resources")
                                        .getKey("/XObject")
                                        .getKey("/Fx1"));
    std::cout << "--- parseContents ---" << std::endl;
    ParserCallbacks cb;
    fx1.parseContents(&cb);
    // Do this once with addContentTokenFilter and once with
    // addTokenFilter to show that they are the same and to ensure
    // that addTokenFilter is directly exercised in testing.
    for (int i = 0; i < 2; i++) {
        Pl_Buffer b("buffer");
        if (i == 0) {
            fx1.addContentTokenFilter(
                std::shared_ptr<QPDFObjectHandle::TokenFilter>(
                    new TokenFilter()));
        } else {
            fx1.getObjectHandle().addTokenFilter(
                std::shared_ptr<QPDFObjectHandle::TokenFilter>(
                    new TokenFilter()));
        }
        fx1.pipeContents(&b);
        std::unique_ptr<Buffer> buf(b.getBuffer());
        std::string s(
            reinterpret_cast<char const*>(buf->getBuffer()), buf->getSize());
        assert(s.find("/bye") != std::string::npos);
    }
}

static void
test_73(QPDF& pdf, char const* arg2)
{
    try {
        QPDF pdf2;
        pdf2.getRoot();
    } catch (std::exception& e) {
        std::cerr << "getRoot: " << e.what() << std::endl;
    }

    pdf.closeInputSource();
    pdf.getRoot().getKey("/Pages").unparseResolved();
}

static void
test_74(QPDF& pdf, char const* arg2)
{
    // This test is crafted to work with split-nntree.pdf
    std::cout << "/Split1" << std::endl;
    auto split1 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Split1"), pdf);
    split1.setSplitThreshold(4);
    auto check_split1 = [&split1](int k) {
        auto i = split1.insert(
            k, QPDFObjectHandle::newString(QUtil::int_to_string(k)));
        assert(i->first == k);
    };
    check_split1(15);
    check_split1(35);
    check_split1(125);
    for (auto const& i: split1) {
        std::cout << i.first << std::endl;
    }

    std::cout << "/Split2" << std::endl;
    auto split2 =
        QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Split2"), pdf);
    split2.setSplitThreshold(4);
    auto check_split2 = [](QPDFNameTreeObjectHelper& noh,
                           std::string const& k) {
        auto i = noh.insert(k, QPDFObjectHandle::newUnicodeString(k));
        assert(i->first == k);
    };
    check_split2(split2, "C");
    for (auto const& i: split2) {
        std::cout << i.first << std::endl;
    }

    std::cout << "/Split3" << std::endl;
    auto split3 =
        QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Split3"), pdf);
    split3.setSplitThreshold(4);
    check_split2(split3, "P");
    check_split2(split3, "\xcf\x80");
    for (auto& i: split3) {
        std::cout << i.first << " " << i.second.unparse() << std::endl;
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setQDFMode(true);
    w.write();
}

static void
test_75(QPDF& pdf, char const* arg2)
{
    // This test is crafted to work with erase-nntree.pdf
    auto erase1 =
        QPDFNameTreeObjectHelper(pdf.getTrailer().getKey("/Erase1"), pdf);
    QPDFObjectHandle value;
    assert(!erase1.remove("1X"));
    assert(erase1.remove("1C", &value));
    assert(value.getUTF8Value() == "c");
    auto iter1 = erase1.find("1B");
    iter1.remove();
    assert(iter1->first == "1D");
    iter1.remove();
    assert(iter1 == erase1.end());
    --iter1;
    assert(iter1->first == "1A");
    iter1.remove();
    assert(iter1 == erase1.end());

    auto erase2_oh = pdf.getTrailer().getKey("/Erase2");
    auto erase2 = QPDFNumberTreeObjectHelper(erase2_oh, pdf);
    auto iter2 = erase2.find(250);
    iter2.remove();
    assert(iter2 == erase2.end());
    --iter2;
    assert(iter2->first == 240);
    auto k1 = erase2_oh.getKey("/Kids").getArrayItem(1);
    auto l1 = k1.getKey("/Limits");
    assert(l1.getArrayItem(0).getIntValue() == 230);
    assert(l1.getArrayItem(1).getIntValue() == 240);
    iter2 = erase2.find(210);
    iter2.remove();
    assert(iter2->first == 220);
    k1 = erase2_oh.getKey("/Kids").getArrayItem(0);
    l1 = k1.getKey("/Limits");
    assert(l1.getArrayItem(0).getIntValue() == 220);
    assert(l1.getArrayItem(1).getIntValue() == 220);
    k1 = k1.getKey("/Kids");
    assert(k1.getArrayNItems() == 1);

    auto erase3 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Erase3"), pdf);
    iter2 = erase3.find(320);
    iter2.remove();
    assert(iter2 == erase3.end());
    erase3.remove(310);
    assert(erase3.begin() == erase3.end());

    auto erase4 =
        QPDFNumberTreeObjectHelper(pdf.getTrailer().getKey("/Erase4"), pdf);
    iter2 = erase4.find(420);
    iter2.remove();
    assert(iter2->first == 430);

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setQDFMode(true);
    w.write();
}

static void
test_76(QPDF& pdf, char const* arg2)
{
    // Embedded files. arg2 is a file to attach. Hard-code the
    // mime type and file name for test purposes.
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    auto fs1 = QPDFFileSpecObjectHelper::createFileSpec(pdf, "att1.txt", arg2);
    fs1.setDescription("some text");
    auto efs1 = QPDFEFStreamObjectHelper(fs1.getEmbeddedFileStream());
    efs1.setSubtype("text/plain")
        .setCreationDate("D:20210207191121-05'00'")
        .setModDate("D:20210208001122Z");
    efdh.replaceEmbeddedFile("att1", fs1);
    auto efs2 = QPDFEFStreamObjectHelper::createEFStream(pdf, "from string");
    efs2.setSubtype("text/plain");
    Pl_Buffer p("buffer");
    // exercise Pipeline::operator<<(std::string const&)
    p << std::string("from buffer");
    p.finish();
    auto efs3 = QPDFEFStreamObjectHelper::createEFStream(
        pdf, p.getBufferSharedPointer());
    efs3.setSubtype("text/plain");
    efdh.replaceEmbeddedFile(
        "att2",
        QPDFFileSpecObjectHelper::createFileSpec(pdf, "att2.txt", efs2));
    auto fs3 = QPDFFileSpecObjectHelper::createFileSpec(pdf, "att3.txt", efs3);
    efdh.replaceEmbeddedFile("att3", fs3);
    fs3.setFilename("\xcf\x80.txt", "att3.txt");

    assert(efs1.getCreationDate() == "D:20210207191121-05'00'");
    assert(efs1.getModDate() == "D:20210208001122Z");
    assert(efs2.getSize() == 11);
    assert(efs2.getSubtype() == "text/plain");
    assert(
        QUtil::hex_encode(efs2.getChecksum()) ==
        "2fce9c8228e360ba9b04a1bd1bf63d6b");

    for (auto iter: efdh.getEmbeddedFiles()) {
        std::cout << iter.first << " -> " << iter.second->getFilename()
                  << std::endl;
    }
    assert(efdh.getEmbeddedFile("att1")->getFilename() == "att1.txt");
    assert(!efdh.getEmbeddedFile("potato"));

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setQDFMode(true);
    w.write();
}

static void
test_77(QPDF& pdf, char const* arg2)
{
    QPDFEmbeddedFileDocumentHelper efdh(pdf);
    assert(efdh.removeEmbeddedFile("att2"));
    assert(!efdh.removeEmbeddedFile("att2"));

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setQDFMode(true);
    w.write();
}

static void
test_78(QPDF& pdf, char const* arg2)
{
    // Test functional versions of replaceStreamData()

    auto f1 = [](Pipeline* p) {
        p->writeCStr("potato");
        p->finish();
    };
    auto f2 = [](Pipeline* p, bool suppress_warnings, bool will_retry) {
        std::cerr << "f2" << std::endl;
        if (will_retry) {
            std::cerr << "failing" << std::endl;
            return false;
        }
        if (!suppress_warnings) {
            std::cerr << "warning" << std::endl;
        }
        p->writeCStr("salad");
        p->finish();
        std::cerr << "f2 done" << std::endl;
        return true;
    };

    auto null = QPDFObjectHandle::newNull();
    auto s1 = QPDFObjectHandle::newStream(&pdf);
    s1.replaceStreamData(f1, null, null);
    auto s2 = QPDFObjectHandle::newStream(&pdf);
    s2.replaceStreamData(f2, null, null);
    pdf.getTrailer().replaceKey(
        "/Streams", QPDFObjectHandle::newArray({s1, s2}));
    std::cout << "piping with warning suppression" << std::endl;
    Pl_Discard d;
    s2.pipeStreamData(&d, nullptr, 0, qpdf_dl_all, true, false);

    std::cout << "writing" << std::endl;
    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setQDFMode(true);
    w.write();
}

static void
test_79(QPDF& pdf, char const* arg2)
{
    // Exercise stream copier

    // Copy streams. Modify the original and make sure the copy is
    // unaffected.
    auto copies = QPDFObjectHandle::newArray();
    pdf.getTrailer().replaceKey("/Copies", copies);
    auto null = QPDFObjectHandle::newNull();

    // Get a regular stream from the file
    auto p1 = pdf.getAllPages().at(0);
    auto s1 = p1.getKey("/Contents");

    // Create a stream from a string
    auto s2 = QPDFObjectHandle::newStream(&pdf, "from string");
    // Add direct and indirect objects to the dictionary
    s2.getDict().replaceKey(
        "/Stuff",
        QPDFObjectHandle::parse(
            &pdf,
            "<< /Direct 3 /Indirect " +
                pdf.makeIndirectObject(QPDFObjectHandle::newInteger(16059))
                    .unparse() +
                ">>"));
    s2.getDict().replaceKey(
        "/Other", QPDFObjectHandle::newString("other stuff"));

    // Use a provider
    Pl_Buffer b("buffer");
    b.writeCStr("from buffer");
    b.finish();
    auto bp = b.getBufferSharedPointer();
    auto s3 = QPDFObjectHandle::newStream(&pdf, bp);

    std::vector<QPDFObjectHandle> streams = {s1, s2, s3};
    pdf.getTrailer().replaceKey(
        "/Originals", QPDFObjectHandle::newArray(streams));

    int i = 0;
    for (auto orig: streams) {
        ++i;
        auto istr = QUtil::int_to_string(i);
        auto orig_data = orig.getStreamData();
        auto copy = orig.copyStream();
        copy.getDict().replaceKey(
            "/Other", QPDFObjectHandle::newString("other: " + istr));
        orig.replaceStreamData("something new " + istr, null, null);
        auto copy_data = copy.getStreamData();
        assert(orig_data->getSize() == copy_data->getSize());
        assert(
            memcmp(
                orig_data->getBuffer(),
                copy_data->getBuffer(),
                orig_data->getSize()) == 0);
        copies.appendItem(copy);
    }

    QPDFWriter w(pdf, "a.pdf");
    w.setStaticID(true);
    w.setQDFMode(true);
    w.write();
}

static void
test_80(QPDF& pdf, char const* arg2)
{
    // Exercise transform/copy annotations without passing in
    // QPDFAcroFormDocumentHelper pointers. The case of passing
    // them in is sufficiently exercised by testing through the
    // qpdf CLI.

    // The main file is a file that has lots of annotations. Arg2
    // is a file to copy annotations to.

    QPDFMatrix m;
    m.translate(306, 396);
    m.scale(0.4, 0.4);
    auto page1 = pdf.getAllPages().at(0);
    auto old_annots = page1.getKey("/Annots");
    // Transform annotations and copy them back to the same page.
    std::vector<QPDFObjectHandle> new_annots;
    std::vector<QPDFObjectHandle> new_fields;
    std::set<QPDFObjGen> old_fields;
    QPDFAcroFormDocumentHelper afdh(pdf);
    // Use defaults for from_qpdf and from_afdh.
    afdh.transformAnnotations(
        old_annots, new_annots, new_fields, old_fields, m);
    for (auto const& annot: new_annots) {
        old_annots.appendItem(annot);
    }
    afdh.addAndRenameFormFields(new_fields);

    m = QPDFMatrix();
    m.translate(612, 0);
    m.scale(-1, 1);
    QPDF pdf2;
    pdf2.processFile(arg2);
    auto page2 = QPDFPageDocumentHelper(pdf2).getAllPages().at(0);
    page2.copyAnnotations(page1, m);

    QPDFWriter w1(pdf, "a.pdf");
    w1.setStaticID(true);
    w1.setQDFMode(true);
    w1.write();

    QPDFWriter w2(pdf2, "b.pdf");
    w2.setStaticID(true);
    w2.setQDFMode(true);
    w2.write();
}

static void
test_81(QPDF& pdf, char const* arg2)
{
    // Exercise that type errors get their own special type
    try {
        QPDFObjectHandle::newNull().getIntValue();
        assert(false);
    } catch (QPDFExc& e) {
        assert(e.getErrorCode() == qpdf_e_object);
    }
}

static void
test_82(QPDF& pdf, char const* arg2)
{
    // Exercise compound test methods QPDFObjectHandle::isNameAndEquals,
    // isDictionaryOfType and isStreamOfType
    auto name = QPDFObjectHandle::newName("/Marvin");
    auto str = QPDFObjectHandle::newString("/Marvin");
    assert(name.isNameAndEquals("/Marvin"));
    assert(!name.isNameAndEquals("Marvin"));
    assert(!str.isNameAndEquals("/Marvin"));
    auto dict =
        QPDFObjectHandle::parse("<</A 1 /Type /Test /Subtype /Marvin>>");
    assert(dict.isDictionaryOfType("/Test", ""));
    assert(dict.isDictionaryOfType("/Test"));
    assert(dict.isDictionaryOfType("/Test", "/Marvin"));
    assert(dict.isDictionaryOfType("", "/Marvin"));
    assert(dict.isDictionaryOfType("", ""));
    assert(!dict.isDictionaryOfType("/Test2", ""));
    assert(!dict.isDictionaryOfType("/Test2", "/Marvin"));
    assert(!dict.isDictionaryOfType("/Test", "/M"));
    assert(!name.isDictionaryOfType("", ""));
    dict = QPDFObjectHandle::parse("<</A 1 /Type null /Subtype /Marvin>>");
    assert(!dict.isDictionaryOfType("/Test"));
    dict = QPDFObjectHandle::parse("<</A 1 /Type (Test) /Subtype /Marvin>>");
    assert(!dict.isDictionaryOfType("Test"));
    dict = QPDFObjectHandle::parse("<</A 1 /Type /Test /Subtype (Marvin)>>");
    assert(!dict.isDictionaryOfType("Test"));
    dict = QPDFObjectHandle::parse("<</A 1 /Subtype /Marvin>>");
    assert(!dict.isDictionaryOfType("/Test", "Marvin"));
    auto stream = pdf.getObjectByID(1, 0);
    assert(stream.isStreamOfType("/ObjStm"));
    assert(!stream.isStreamOfType("/Test"));
    assert(!pdf.getObjectByID(2, 0).isStreamOfType("/Pages"));
    /* cSpell: ignore Blaah Blaaah Blaaaah */
    auto array = QPDFObjectHandle::parse("[/Blah /Blaah /Blaaah]");
    assert(array.isOrHasName("/Blah"));
    assert(array.isOrHasName("/Blaaah"));
    assert(!array.isOrHasName("/Blaaaah"));
    assert(array.getArrayItem(0).isOrHasName("/Blah"));
    assert(!array.getArrayItem(1).isOrHasName("/Blah"));
    array = QPDFObjectHandle::parse("[]");
    assert(!array.isOrHasName("/Blah"));
    assert(!str.isOrHasName("/Marvin"));
}

static void
test_83(QPDF& pdf, char const* arg2)
{
    // Test QPDFJob json with partial = false. For testing with
    // partial = true, we just use qpdf --job-json-file.

    QPDFJob j;
    std::shared_ptr<char> file_buf;
    size_t size;
    QUtil::read_file_into_memory(arg2, file_buf, size);
    try {
        std::cout << "calling initializeFromJson" << std::endl;
        j.initializeFromJson(std::string(file_buf.get(), size));
        std::cout << "called initializeFromJson" << std::endl;
    } catch (QPDFUsage& e) {
        std::cerr << "usage: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
}

static void
test_84(QPDF& pdf, char const* arg2)
{
    // Test QPDFJob API

    std::cout << "normal" << std::endl;
    {
        QPDFJob j;
        j.config()
            ->inputFile("minimal.pdf")
            ->outputFile("a.pdf")
            ->qdf()
            ->deterministicId()
            ->objectStreams("preserve")
            ->progress()
            ->checkConfiguration();
        j.run();
        assert(j.getExitCode() == 0);
        assert(!j.hasWarnings());
        assert(j.getEncryptionStatus() == 0);
    }

    std::cout << "custom progress reporter" << std::endl;
    {
        QPDFJob j;
        j.registerProgressReporter([](int p) {
            std::cout << "custom write progress: " << p << "%" << std::endl;
        });
        j.config()
            ->inputFile("minimal.pdf")
            ->outputFile("a.pdf")
            ->qdf()
            ->deterministicId()
            ->objectStreams("preserve")
            ->progress()
            ->checkConfiguration();
        j.run();
        assert(j.getExitCode() == 0);
        assert(!j.hasWarnings());
        assert(j.getEncryptionStatus() == 0);
    }

    std::cout << "error caught by check" << std::endl;
    try {
        QPDFJob j;
        j.config()->outputFile("a.pdf")->qdf();
        std::cout << "finished config" << std::endl;
        j.checkConfiguration();
        assert(false);
    } catch (QPDFUsage& e) {
        std::cout << "usage: " << e.what() << std::endl;
    }

    std::cout << "error caught by run" << std::endl;
    try {
        QPDFJob j;
        j.config()->outputFile("a.pdf")->qdf();
        std::cout << "finished config" << std::endl;
        j.run();
        assert(false);
    } catch (QPDFUsage& e) {
        std::cout << "usage: " << e.what() << std::endl;
    }

    std::cout << "output capture" << std::endl;
    std::ostringstream cout;
    std::ostringstream cerr;
    {
        QPDFJob j;
#ifdef _MSC_VER
# pragma warning(disable : 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        j.setOutputStreams(&cout, &cerr);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
        j.config()
            ->inputFile("bad2.pdf")
            ->showObject("4,0")
            ->checkConfiguration();
        std::cout << "calling run" << std::endl;
        j.run();
        std::cout << "captured stdout" << std::endl;
        std::cout << cout.str();
        std::cout << "captured stderr" << std::endl;
        std::cout << cerr.str();
    }
}

static void
test_85(QPDF& pdf, char const* arg2)
{
    // Test QPDFObjectHandle::getValueAs... accessors

    auto oh_b = QPDFObjectHandle::newBool(false);
    auto oh_i = QPDFObjectHandle::newInteger(1);
    auto oh_i_maxplus =
        QPDFObjectHandle::newInteger(QIntC::to_longlong(INT_MAX) + 1LL);
    auto oh_i_umaxplus =
        QPDFObjectHandle::newInteger(QIntC::to_longlong(UINT_MAX) + 1LL);
    auto oh_i_minminus =
        QPDFObjectHandle::newInteger(QIntC::to_longlong(INT_MIN) - 1LL);
    auto oh_i_neg = QPDFObjectHandle::newInteger(-1);
    auto oh_r = QPDFObjectHandle::newReal("42.0");
    auto oh_n = QPDFObjectHandle::newName("/Test");
    auto oh_s = QPDFObjectHandle::newString("/Test");
    auto oh_o = QPDFObjectHandle::newOperator("/Test");
    auto oh_ii = QPDFObjectHandle::newInlineImage("/Test");

    bool b = true;
    assert(oh_b.getValueAsBool(b));
    assert(!b);
    assert(!oh_i.getValueAsBool(b));
    assert(!b);
    long long li = 0LL;
    assert(oh_i.getValueAsInt(li));
    assert(li == 1LL);
    assert(!oh_b.getValueAsInt(li));
    assert(li == 1LL);
    int i = 0;
    assert(oh_i.getValueAsInt(i));
    assert(i == 1);
    assert(!oh_b.getValueAsInt(i));
    assert(i == 1);
    assert(oh_i_maxplus.getValueAsInt(i));
    assert(i == INT_MAX);
    assert(oh_i_minminus.getValueAsInt(i));
    assert(i == INT_MIN);
    unsigned long long uli = 0U;
    assert(oh_i.getValueAsUInt(uli));
    assert(uli == 1u);
    assert(!oh_b.getValueAsUInt(uli));
    assert(uli == 1u);
    assert(oh_i_neg.getValueAsUInt(uli));
    assert(uli == 0u);
    unsigned int ui = 0U;
    assert(oh_i.getValueAsUInt(ui));
    assert(ui == 1u);
    assert(!oh_b.getValueAsUInt(ui));
    assert(ui == 1u);
    assert(oh_i_neg.getValueAsUInt(ui));
    assert(ui == 0u);
    assert(oh_i_umaxplus.getValueAsUInt(ui));
    assert(ui == UINT_MAX);
    std::string s = "0";
    assert(oh_r.getValueAsReal(s));
    assert(s == "42.0");
    assert(!oh_i.getValueAsReal(s));
    assert(s == "42.0");
    double num = 0.0;
    assert(oh_i.getValueAsNumber(num));
    assert(((num - 1.0) < 1e-6) && (num - 1.0 > -1e-6));
    assert(oh_r.getValueAsNumber(num));
    assert(((num - 42.0) < 1e-6) && (num - 42.0 > -1e-6));
    assert(!oh_b.getValueAsNumber(num));
    assert(((num - 42.0) < 1e-6) && (num - 42.0 > -1e-6));
    s = "";
    assert(oh_n.getValueAsName(s));
    assert(s == "/Test");
    assert(!oh_r.getValueAsName(s));
    assert(s == "/Test");
    s = "";
    assert(oh_s.getValueAsUTF8(s));
    assert(s == "/Test");
    assert(!oh_r.getValueAsUTF8(s));
    assert(s == "/Test");
    s = "";
    assert(oh_s.getValueAsUTF8(s));
    assert(s == "/Test");
    assert(!oh_r.getValueAsUTF8(s));
    assert(s == "/Test");
    s = "";
    assert(oh_o.getValueAsOperator(s));
    assert(s == "/Test");
    assert(!oh_r.getValueAsOperator(s));
    assert(s == "/Test");
    s = "";
    assert(oh_ii.getValueAsInlineImage(s));
    assert(s == "/Test");
    assert(!oh_r.getValueAsInlineImage(s));
    assert(s == "/Test");
}

static void
test_86(QPDF& pdf, char const* arg2)
{
    // Test symmetry between newUnicodeString and getUTF8Value for
    // strings that can't be encoded as PDFDoc but don't contain any
    // high code points.

    std::string utf8_val("\x1f");
    std::string utf16_val("\xfe\xff\x00\x1f", 4);
    std::string result;
    assert(QUtil::utf8_to_ascii(utf8_val, result, '?'));
    assert(result == utf8_val);
    assert(!QUtil::utf8_to_pdf_doc(utf8_val, result, '?'));
    assert(result == "?");
    assert(QUtil::utf8_to_utf16(utf8_val) == utf16_val);
    assert(QUtil::utf16_to_utf8(utf16_val) == utf8_val);
    auto h = QPDFObjectHandle::newUnicodeString(utf8_val);
    assert(h.getStringValue() == utf16_val);
    assert(h.getUTF8Value() == utf8_val);
}

static void
test_87(QPDF& pdf, char const* arg2)
{
    // Explicitly demonstrate null dictionary values being the same as
    // missing keys.
    auto dict = "<< /A 1 /B null >>"_qpdf;
    assert(dict.unparse() == "<< /A 1 >>");
    assert(dict.getKeys() == std::set<std::string>({"/A"}));
    dict.replaceKey("/A", QPDFObjectHandle::newNull());
    assert(dict.unparse() == "<< >>");
    assert(dict.getKeys() == std::set<std::string>());
    dict = QPDFObjectHandle::newDictionary({
        {"/A", "2"_qpdf},
        {"/B", QPDFObjectHandle::newNull()},
    });
    assert(dict.unparse() == "<< /A 2 >>");
    assert(dict.getKeys() == std::set<std::string>({"/A"}));
    assert(dict.getJSON(JSON::LATEST).unparse() == "{\n  \"/A\": 2\n}");
}

static void
test_88(QPDF& pdf, char const* arg2)
{
    // Exercise mutate and get methods added for qpdf 11.
    auto dict = QPDFObjectHandle::newDictionary();
    dict.replaceKey("/One", QPDFObjectHandle::newInteger(1));
    dict.replaceKey("/Two", QPDFObjectHandle::newInteger(2));
    auto three =
        dict.replaceKeyAndGetNew("/Three", QPDFObjectHandle::newArray());
    three.appendItem("(a)"_qpdf);
    three.appendItem("(b)"_qpdf);
    auto newdict = three.appendItemAndGetNew(QPDFObjectHandle::newDictionary());
    newdict.replaceKey("/Z", "/Y"_qpdf);
    newdict.replaceKey("/X", "/W"_qpdf);
    dict.replaceKey("/Quack", "[1 2 3]"_qpdf);
    auto quack = dict.replaceKeyAndGetOld("/Quack", "/Moo"_qpdf);
    assert(quack.unparse() == "[ 1 2 3 ]");
    auto nothing =
        dict.replaceKeyAndGetOld("/NotThere", QPDFObjectHandle::newNull());
    assert(nothing.isNull());
    assert(dict.unparse() == R"(
      <<
        /One 1
        /Quack /Moo
        /Two 2
        /Three [ (a) (b) << /Z /Y /X /W >> ]
      >>
    )"_qpdf.unparse());
    auto arr = dict.getKey("/Three");
    arr.insertItem(0, QPDFObjectHandle::newString("0"));
    arr.insertItem(0, QPDFObjectHandle::newString("00"));
    assert(
        arr.unparse() ==
        "[ (00) (0) (a) (b) << /Z /Y /X /W >> ]"_qpdf.unparse());
    auto new_dict = arr.insertItemAndGetNew(1, "<< /P /Q /R /S >>"_qpdf);
    arr.eraseItem(2);
    arr.eraseItem(0);
    assert(
        arr.unparse() ==
        "[ << /P /Q /R /S >> (a) (b) << /Z /Y /X /W >> ]"_qpdf.unparse());

    // new_dict shares internals with the one in the array. It has
    // always been this way, and there is code that relies on this
    // behavior. Maybe it would be different if I could start over
    // again...
    new_dict.removeKey("/R");
    new_dict.replaceKey("/T", "/U"_qpdf);
    assert(
        arr.unparse() ==
        "[ << /P /Q /T /U >> (a) (b) << /Z /Y /X /W >> ]"_qpdf.unparse());
    auto s = arr.eraseItemAndGetOld(1);
    assert(s.unparse() == "(a)");
    assert(
        arr.unparse() ==
        "[ << /P /Q /T /U >> (b) << /Z /Y /X /W >> ]"_qpdf.unparse());

    assert(new_dict.removeKeyAndGetOld("/M").isNull());
    assert(new_dict.removeKeyAndGetOld("/P").unparse() == "/Q");
    assert(new_dict.unparse() == "<< /T /U >>"_qpdf.unparse());

    // Test errors
    auto arr2 = pdf.getRoot().replaceKeyAndGetNew("/QTest", "[1 2]"_qpdf);
    arr2.setObjectDescription(&pdf, "test array");
    assert(arr2.eraseItemAndGetOld(50).isNull());
}

static void
test_89(QPDF& pdf, char const* arg2)
{
    // Generate object warning with json-input. Crafted to work with
    // manual-qpdf-json.json.
    auto null = QPDFObjectHandle::newNull();
    pdf.getTrailer().appendItem(null);
    pdf.getRoot().appendItem(null);
    pdf.getObjectByID(5, 0).replaceKey("/X", null);
    pdf.getObjectByID(5, 0).getArrayItem(0).replaceKey("/X", null);
}

static void
test_90(QPDF& pdf, char const* arg2)
{
    // Generate object warning with update-from-json. Crafted to work
    // with good13.pdf and various-updates.json. JSON file is arg2.
    pdf.updateFromJSON(arg2);
    pdf.getTrailer().appendItem(QPDFObjectHandle::newNull());
    pdf.getTrailer().getKey("/QTest").appendItem(QPDFObjectHandle::newNull());
    pdf.getTrailer().getKey("/QTest").getKey("/strings").getIntValue();
    // not from json
    pdf.getRoot().appendItem(QPDFObjectHandle::newNull());
}

static void
test_91(QPDF& pdf, char const* arg2)
{
    // Exercise the simpler version of writeJSON.
    Pl_StdioFile p("stdout", stdout);
    pdf.writeJSON(
        2, &p, qpdf_dl_none, qpdf_sj_inline, "", std::set<std::string>());
}

static void
test_92(QPDF& pdf, char const* arg2)
{
    // Exercise indirect objects owned by destroyed QPDF object.
    auto qpdf = QPDF::create();
    qpdf->processFile("minimal.pdf");
    auto root = qpdf->getRoot();
    assert(root.getOwningQPDF() == qpdf.get());
    assert(root.isIndirect());
    assert(root.isDictionary());
    auto page1 = root.getKey("/Pages").getKey("/Kids").getArrayItem(0);
    assert(page1.getOwningQPDF() == qpdf.get());
    assert(page1.isIndirect());
    assert(page1.isDictionary());
    auto resources = page1.getKey("/Resources");
    assert(resources.getOwningQPDF() == qpdf.get());
    assert(resources.isDictionary());
    assert(!resources.isIndirect());
    auto contents = page1.getKey("/Contents");
    auto contents_dict = contents.getDict();
    qpdf = nullptr;
    auto check = [](QPDFObjectHandle& oh) {
        assert(oh.getOwningQPDF() == nullptr);
        assert(!oh.isIndirect());
    };
    // All objects should no longer have an owning QPDF or be indirect.
    check(root);
    check(page1);
    check(resources);
    check(contents);
    check(contents_dict);
    // Objects that were originally indirect should be destroyed.
    // Otherwise, they should have retained their old values but just
    // lost their connection to the owning QPDF.
    assert(root.isDestroyed());
    assert(page1.isDestroyed());
    assert(contents.isDestroyed());
    assert(resources.isDictionary());
    assert(contents_dict.isDictionary());
    try {
        root.unparse();
        assert(false);
    } catch (std::logic_error&) {
        // Expected
    }
}

static void
test_93(QPDF& pdf, char const* arg2)
{
    // Test QPDFObjectHandle equality. Two QPDFObjectHandle objects
    // are equal if they point to the same underlying object.

    auto trailer = pdf.getTrailer();
    auto root1 = trailer.getKey("/Root");
    auto root2 = pdf.getRoot();
    assert(root1.isSameObjectAs(root2));
    auto oh1 = "<< /One /Two >>"_qpdf;
    auto oh2 = oh1;
    assert(oh1.isSameObjectAs(oh2));
    auto oh3 = "<< /One /Two >>"_qpdf;
    assert(!oh1.isSameObjectAs(oh3));
    oh2.replaceKey("/One", "/Three"_qpdf);
    assert(oh1.isSameObjectAs(oh2));
    assert(oh2.unparse() == "<< /One /Three >>");
    assert(!oh1.isIndirect());
    auto oh4 = pdf.makeIndirectObject(oh1);
    assert(oh1.isSameObjectAs(oh4));
    assert(oh1.isIndirect());
    assert(oh4.isIndirect());
    trailer.replaceKey("/Potato", oh1);
    assert(trailer.getKey("/Potato").isSameObjectAs(oh2));
}

static void
test_94(QPDF& pdf, char const* arg2)
{
    // Exercise methods to get page boxes. This test is built for
    // boxes2.pdf.

    // /MediaBox is present in the pages tree root.
    // Each page has the following boxes present directly:
    // 1. none
    // 2. crop
    // 3. media, crop
    // 4. media, crop, trim, bleed; crop is indirect
    // 5. trim, art

    auto pages_root = pdf.getRoot().getKey("/Pages");
    auto root_media = pages_root.getKey("/MediaBox");
    auto root_media_unparse = root_media.unparse();
    auto pages = QPDFPageDocumentHelper(pdf).getAllPages();
    assert(pages.size() == 5);
    auto& p1 = pages[0];
    auto& p2 = pages[1];
    auto& p3 = pages[2];
    auto& p4 = pages[3];
    auto& p5 = pages[4];

    assert(p1.getObjectHandle().getKey("/MediaBox").isNull());
    // MediaBox not present, so get inherited one
    assert(p1.getMediaBox(false).isSameObjectAs(root_media));
    // Other boxesBox not present, so fall back to MediaBox
    assert(p1.getCropBox(false, false).isSameObjectAs(root_media));
    assert(p1.getBleedBox(false, false).isSameObjectAs(root_media));
    assert(p1.getTrimBox(false, false).isSameObjectAs(root_media));
    assert(p1.getArtBox(false, false).isSameObjectAs(root_media));
    // Make copy of artbox
    auto p1_new_art = p1.getArtBox(false, true);
    assert(p1_new_art.unparse() == root_media_unparse);
    assert(!p1_new_art.isSameObjectAs(root_media));
    // This also copied cropbox
    auto p1_new_crop = p1.getCropBox(false, false);
    assert(!p1_new_crop.isSameObjectAs(root_media));
    assert(!p1_new_crop.isSameObjectAs(p1_new_art));
    assert(p1_new_crop.unparse() == root_media_unparse);
    // But it didn't copy Media
    assert(p1.getMediaBox(false).isSameObjectAs(root_media));
    // Now fall back to new crop
    assert(p1.getTrimBox(false, false).isSameObjectAs(p1_new_crop));
    // Request copy. The value returned has the same structure but is
    // a different object.
    auto p1_effective_media = p1.getMediaBox(true);
    assert(p1_effective_media.unparse() == root_media_unparse);
    assert(!p1_effective_media.isSameObjectAs(root_media));

    // copy_on_fallback didn't have to copy media to crop
    assert(p2.getMediaBox(false).isSameObjectAs(root_media));
    auto p2_crop = p2.getCropBox(false, false);
    auto p2_new_trim = p2.getTrimBox(false, true);
    assert(p2_new_trim.unparse() == p2_crop.unparse());
    assert(!p2_new_trim.isSameObjectAs(p2_crop));
    assert(p2.getMediaBox(false).isSameObjectAs(root_media));

    // We didn't need to copy anything
    auto p3_media = p3.getMediaBox(false);
    auto p3_crop = p3.getCropBox(false, false);
    assert(p3.getMediaBox(true).isSameObjectAs(p3_media));
    assert(p3.getCropBox(true, true).isSameObjectAs(p3_crop));

    // We didn't have to copy for bleed but we did for art
    auto p4_orig_crop = p4.getObjectHandle().getKey("/CropBox");
    auto p4_crop = p4.getCropBox(false, false);
    assert(p4_orig_crop.isSameObjectAs(p4_crop));
    auto p4_bleed1 = p4.getBleedBox(false, false);
    auto p4_bleed2 = p4.getBleedBox(false, true);
    assert(!p4_bleed1.isSameObjectAs(p4_crop));
    assert(p4_bleed1.isSameObjectAs(p4_bleed2));
    auto p4_art1 = p4.getArtBox(false, false);
    assert(p4_art1.isSameObjectAs(p4_crop));
    auto p4_art2 = p4.getArtBox(false, true);
    assert(!p4_art2.isSameObjectAs(p4_crop));
    auto p4_new_crop = p4.getCropBox(true, false);
    assert(!p4_new_crop.isSameObjectAs(p4_orig_crop));
    assert(p4_orig_crop.isIndirect());
    assert(!p4_new_crop.isIndirect());
    assert(p4_new_crop.unparse() == p4_orig_crop.unparseResolved());

    // Exercise copying for inheritance and fallback
    assert(p5.getMediaBox(false).isSameObjectAs(root_media));
    assert(p5.getCropBox(false, false).isSameObjectAs(root_media));
    assert(p5.getBleedBox(false, false).isSameObjectAs(root_media));
    auto p5_new_bleed = p5.getBleedBox(true, true);
    auto p5_new_media = p5.getMediaBox(false);
    auto p5_new_crop = p5.getCropBox(false, false);
    assert(!p5_new_media.isSameObjectAs(root_media));
    assert(!p5_new_crop.isSameObjectAs(root_media));
    assert(!p5_new_crop.isSameObjectAs(p5_new_media));
    assert(!p5_new_bleed.isSameObjectAs(root_media));
    assert(!p5_new_bleed.isSameObjectAs(p5_new_media));
    assert(!p5_new_bleed.isSameObjectAs(p5_new_crop));
    assert(p5_new_media.unparse() == root_media_unparse);
    assert(p5_new_crop.unparse() == root_media_unparse);
    assert(p5_new_bleed.unparse() == root_media_unparse);
}

void
runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    std::set<int> ignore_filename = {61, 81, 83, 84, 85, 86, 87, 92};

    if (n == 0) {
        // Throw in some random test cases that don't fit anywhere
        // else.  This is in addition to whatever else is going on in
        // test 0.

        // The code to trim user passwords looks for 0x28 (which is
        // "(") since it marks the beginning of the padding.  Exercise
        // the code to make sure it skips over 0x28 characters that
        // aren't part of padding.
        std::string password("1234567890123456789012(45678\x28\xbf\x4e\x5e");
        assert(password.length() == 32);
        QPDF::trim_user_password(password);
        assert(password == "1234567890123456789012(45678");

        QPDFObjectHandle uninitialized;
        assert(uninitialized.getTypeCode() == ::ot_uninitialized);
        assert(strcmp(uninitialized.getTypeName(), "uninitialized") == 0);

        // ABI: until QPDF 12, spot check deprecated constants
        assert(QPDFObject::ot_dictionary == ::ot_dictionary);
        assert(QPDFObject::ot_uninitialized == ::ot_uninitialized);
    }

    QPDF pdf;
    std::shared_ptr<char> file_buf;
    FILE* filep = 0;
    if (n == 0) {
        pdf.setAttemptRecovery(false);
    }
    if (((n == 35) || (n == 36)) && (arg2 != 0)) {
        // arg2 is password
        pdf.processFile(filename1, arg2);
    } else if (n == 45) {
        // Decode obfuscated files. To obfuscated, run the input file
        // through this perl script, and save the result to
        // filename.obfuscated. This pretends that the input was
        // called filename.pdf and that that file contained the
        // deobfuscated version.

        // undef $/;
        // my @str = split('', <STDIN>);
        // for (my $i = 0; $i < scalar(@str); ++$i)
        // {
        //     $str[$i] = chr(ord($str[$i]) ^ 0xcc);
        // }
        // print(join('', @str));

        std::string filename(std::string(filename1) + ".obfuscated");
        size_t size = 0;
        QUtil::read_file_into_memory(filename.c_str(), file_buf, size);
        char* p = file_buf.get();
        for (size_t i = 0; i < size; ++i) {
            p[i] = static_cast<char>(p[i] ^ 0xcc);
        }
        pdf.processMemoryFile(
            (std::string(filename1) + ".pdf").c_str(), p, size);
    } else if (ignore_filename.count(n)) {
        // Ignore filename argument entirely
    } else if (n == 89) {
        pdf.createFromJSON(filename1);
    } else if (n % 2 == 0) {
        if (n % 4 == 0) {
            QTC::TC("qpdf", "exercise processFile(name)");
            pdf.processFile(filename1);
        } else {
            QTC::TC("qpdf", "exercise processFile(FILE*)");
            filep = QUtil::safe_fopen(filename1, "rb");
            pdf.processFile(filename1, filep, false);
        }
    } else {
        QTC::TC("qpdf", "exercise processMemoryFile");
        size_t size = 0;
        QUtil::read_file_into_memory(filename1, file_buf, size);
        pdf.processMemoryFile(filename1, file_buf.get(), size);
    }

    std::map<int, void (*)(QPDF&, char const*)> test_functions = {
        {0, test_0_1}, {1, test_0_1}, {2, test_2},   {3, test_3},
        {4, test_4},   {5, test_5},   {6, test_6},   {7, test_7},
        {8, test_8},   {9, test_9},   {10, test_10}, {11, test_11},
        {12, test_12}, {13, test_13}, {14, test_14}, {15, test_15},
        {16, test_16}, {17, test_17}, {18, test_18}, {19, test_19},
        {20, test_20}, {21, test_21}, {22, test_22}, {23, test_23},
        {24, test_24}, {25, test_25}, {26, test_26}, {27, test_27},
        {28, test_28}, {29, test_29}, {30, test_30}, {31, test_31},
        {32, test_32}, {33, test_33}, {34, test_34}, {35, test_35},
        {36, test_36}, {37, test_37}, {38, test_38}, {39, test_39},
        {40, test_40}, {41, test_41}, {42, test_42}, {43, test_43},
        {44, test_44}, {45, test_45}, {46, test_46}, {47, test_47},
        {48, test_48}, {49, test_49}, {50, test_50}, {51, test_51},
        {52, test_52}, {53, test_53}, {54, test_54}, {55, test_55},
        {56, test_56}, {57, test_57}, {58, test_58}, {59, test_59},
        {60, test_60}, {61, test_61}, {62, test_62}, {63, test_63},
        {64, test_64}, {65, test_65}, {66, test_66}, {67, test_67},
        {68, test_68}, {69, test_69}, {70, test_70}, {71, test_71},
        {72, test_72}, {73, test_73}, {74, test_74}, {75, test_75},
        {76, test_76}, {77, test_77}, {78, test_78}, {79, test_79},
        {80, test_80}, {81, test_81}, {82, test_82}, {83, test_83},
        {84, test_84}, {85, test_85}, {86, test_86}, {87, test_87},
        {88, test_88}, {89, test_89}, {90, test_90}, {91, test_91},
        {92, test_92}, {93, test_93}, {94, test_94}};

    auto fn = test_functions.find(n);
    if (fn == test_functions.end()) {
        throw std::runtime_error(
            std::string("invalid test ") + QUtil::int_to_string(n));
    }
    (fn->second)(pdf, arg2);

    if (filep) {
        fclose(filep);
    }
    std::cout << "test " << n << " done" << std::endl;
}

int
main(int argc, char* argv[])
{
    QUtil::setLineBuf(stdout);
    if ((whoami = strrchr(argv[0], '/')) == NULL) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if ((argc < 3) || (argc > 4)) {
        usage();
    }

    try {
        int n = QUtil::string_to_int(argv[1]);
        char const* filename1 = argv[2];
        char const* arg2 = argv[3];
        runtest(n, filename1, arg2);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

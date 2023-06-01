#include <qpdf/assert_test.h>

// NOTE: This test program doesn't do anything special to enable large
// file support.  This is important since it verifies that programs
// don't have to do anything special -- all the work is done
// internally by the library as long as they don't do their own file
// I/O.

#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Run "test_large_file write small a.pdf" to get a PDF file that you
// can look at in a reader.

// This program reads and writes specially crafted files for testing
// large file support.  In write mode, write a file of npages pages
// where each page contains unique text and a unique image.  The image
// is a binary representation of the page number.  The image contains
// horizontal stripes with light stripes representing 1, dark stripes
// representing 0, and the high bit on top.  In read mode, read the
// file back checking to make sure all the image data and page
// contents are as expected.

// Running this is small mode produces a small file that is easy to
// look at in any viewer.  Since there is no question about proper
// functionality for small files, writing and reading the small file
// allows the qpdf library to test this test program.  Writing and
// reading the large file then allows us to verify large file support
// with confidence.

static char const* whoami = nullptr;

// Height should be a multiple of 10
static size_t const nstripes = 10;
static size_t const stripesize_large = 500;
static size_t const stripesize_small = 5;
static size_t const npages = 200;

// initialized in main
size_t stripesize = 0;
size_t width = 0;
size_t height = 0;
static unsigned char* buf = nullptr;

static inline unsigned char
get_pixel_color(size_t n, size_t row)
{
    return (
        (n & (1LLU << (nstripes - 1LLU - row))) ? static_cast<unsigned char>('\xc0')
                                                : static_cast<unsigned char>('\x40'));
}

class ImageChecker: public Pipeline
{
  public:
    ImageChecker(size_t n);
    virtual ~ImageChecker() = default;
    virtual void write(unsigned char const* data, size_t len);
    virtual void finish();

  private:
    size_t n;
    size_t offset{0};
    bool okay{true};
};

ImageChecker::ImageChecker(size_t n) :
    Pipeline("image checker", nullptr),
    n(n)
{
}

void
ImageChecker::write(unsigned char const* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        size_t y = (this->offset + i) / width / stripesize;
        unsigned char color = get_pixel_color(n, y);
        if (data[i] != color) {
            okay = false;
        }
    }
    this->offset += len;
}

void
ImageChecker::finish()
{
    if (!okay) {
        std::cout << "errors found checking image data for page " << n << std::endl;
    }
}

class ImageProvider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    ImageProvider(size_t n);
    virtual ~ImageProvider() = default;
    virtual void provideStreamData(int objid, int generation, Pipeline* pipeline);

  private:
    size_t n;
};

ImageProvider::ImageProvider(size_t n) :
    n(n)
{
}

void
ImageProvider::provideStreamData(int objid, int generation, Pipeline* pipeline)
{
    if (buf == nullptr) {
        buf = new unsigned char[width * stripesize];
    }
    std::cout << "page " << n << " of " << npages << std::endl;
    for (size_t y = 0; y < nstripes; ++y) {
        unsigned char color = get_pixel_color(n, y);
        memset(buf, color, width * stripesize);
        pipeline->write(buf, width * stripesize);
    }
    pipeline->finish();
}

void
usage()
{
    std::cerr << "Usage: " << whoami << " {read|write} {large|small} outfile" << std::endl;
    exit(2);
}

static void
set_parameters(bool large)
{
    stripesize = large ? stripesize_large : stripesize_small;
    height = nstripes * stripesize;
    width = height;
}

std::string
generate_page_contents(size_t pageno)
{
    std::string contents = "BT /F1 24 Tf 72 720 Td (page " + QUtil::uint_to_string(pageno) +
        ") Tj ET\n"
        "q 468 0 0 468 72 72 cm /Im1 Do Q\n";
    return contents;
}

static QPDFObjectHandle
create_page_contents(QPDF& pdf, size_t pageno)
{
    return QPDFObjectHandle::newStream(&pdf, generate_page_contents(pageno));
}

QPDFObjectHandle
newName(std::string const& name)
{
    return QPDFObjectHandle::newName(name);
}

QPDFObjectHandle
newInteger(size_t val)
{
    return QPDFObjectHandle::newInteger(QIntC::to_longlong(val));
}

static void
create_pdf(char const* filename)
{
    QPDF pdf;

    pdf.emptyPDF();

    QPDFObjectHandle font = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    font.replaceKey("/Type", newName("/Font"));
    font.replaceKey("/Subtype", newName("/Type1"));
    font.replaceKey("/Name", newName("/F1"));
    font.replaceKey("/BaseFont", newName("/Helvetica"));
    font.replaceKey("/Encoding", newName("/WinAnsiEncoding"));

    QPDFObjectHandle procset = pdf.makeIndirectObject(QPDFObjectHandle::newArray());
    procset.appendItem(newName("/PDF"));
    procset.appendItem(newName("/Text"));
    procset.appendItem(newName("/ImageC"));

    QPDFObjectHandle rfont = QPDFObjectHandle::newDictionary();
    rfont.replaceKey("/F1", font);

    QPDFObjectHandle mediabox = QPDFObjectHandle::newArray();
    mediabox.appendItem(newInteger(0));
    mediabox.appendItem(newInteger(0));
    mediabox.appendItem(newInteger(612));
    mediabox.appendItem(newInteger(792));

    QPDFPageDocumentHelper dh(pdf);
    for (size_t pageno = 1; pageno <= npages; ++pageno) {
        QPDFObjectHandle image = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle image_dict = image.getDict();
        image_dict.replaceKey("/Type", newName("/XObject"));
        image_dict.replaceKey("/Subtype", newName("/Image"));
        image_dict.replaceKey("/ColorSpace", newName("/DeviceGray"));
        image_dict.replaceKey("/BitsPerComponent", newInteger(8));
        image_dict.replaceKey("/Width", newInteger(width));
        image_dict.replaceKey("/Height", newInteger(height));
        auto* p = new ImageProvider(pageno);
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider(p);
        image.replaceStreamData(provider, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());

        QPDFObjectHandle xobject = QPDFObjectHandle::newDictionary();
        xobject.replaceKey("/Im1", image);

        QPDFObjectHandle resources = QPDFObjectHandle::newDictionary();
        resources.replaceKey("/ProcSet", procset);
        resources.replaceKey("/Font", rfont);
        resources.replaceKey("/XObject", xobject);

        QPDFObjectHandle contents = create_page_contents(pdf, pageno);

        QPDFObjectHandle page = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        page.replaceKey("/Type", newName("/Page"));
        page.replaceKey("/MediaBox", mediabox);
        page.replaceKey("/Contents", contents);
        page.replaceKey("/Resources", resources);

        dh.addPage(page, false);
    }

    QPDFWriter w(pdf, filename);
    w.setStaticID(true); // for testing only
    w.setStreamDataMode(qpdf_s_preserve);
    w.setObjectStreamMode(qpdf_o_disable);
    w.write();
}

static void
check_page_contents(size_t pageno, QPDFObjectHandle page)
{
    std::shared_ptr<Buffer> buf = page.getKey("/Contents").getStreamData();
    std::string actual_contents =
        std::string(reinterpret_cast<char*>(buf->getBuffer()), buf->getSize());
    std::string expected_contents = generate_page_contents(pageno);
    if (expected_contents != actual_contents) {
        std::cout << "page contents wrong for page " << pageno << std::endl
                  << "ACTUAL: " << actual_contents << "EXPECTED: " << expected_contents << "----\n";
    }
}

static void
check_image(size_t pageno, QPDFObjectHandle page)
{
    QPDFObjectHandle image = page.getKey("/Resources").getKey("/XObject").getKey("/Im1");
    ImageChecker ic(pageno);
    image.pipeStreamData(&ic, 0, qpdf_dl_specialized);
}

static void
check_pdf(char const* filename)
{
    QPDF pdf;
    pdf.processFile(filename);
    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
    assert(pages.size() == QIntC::to_size(npages));
    for (size_t i = 0; i < npages; ++i) {
        size_t pageno = i + 1;
        std::cout << "page " << pageno << " of " << npages << std::endl;
        check_page_contents(pageno, pages.at(i));
        check_image(pageno, pages.at(i));
    }
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);

    if (argc != 4) {
        usage();
    }
    char const* operation = argv[1];
    char const* size = argv[2];
    char const* filename = argv[3];

    bool op_write = false;
    bool size_large = false;

    if (strcmp(operation, "write") == 0) {
        op_write = true;
    } else if (strcmp(operation, "read") == 0) {
        op_write = false;
    } else {
        usage();
    }

    if (strcmp(size, "large") == 0) {
        size_large = true;
    } else if (strcmp(size, "small") == 0) {
        size_large = false;
    } else {
        usage();
    }

    set_parameters(size_large);

    try {
        if (op_write) {
            create_pdf(filename);
        } else {
            check_pdf(filename);
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }

    delete[] buf;

    return 0;
}

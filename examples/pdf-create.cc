//
// This is an example of creating a PDF file from scratch. It
// illustrates use of several QPDF operations for creating objects and
// streams. It also serves as an illustration of how to use
// StreamDataProvider with different types of filters.
//

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <string.h>

static char const* whoami = nullptr;

// This is a simple StreamDataProvider that writes image data for an
// orange square of the given width and height.
class ImageProvider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    ImageProvider(std::string const& color_space, std::string const& filter);
    virtual ~ImageProvider() = default;
    virtual void provideStreamData(QPDFObjGen const&, Pipeline* pipeline);
    size_t getWidth() const;
    size_t getHeight() const;

  private:
    size_t width;
    size_t stripe_height;
    std::string color_space;
    std::string filter;
    size_t n_stripes;
    std::vector<std::string> stripes;
    J_COLOR_SPACE j_color_space;
};

ImageProvider::ImageProvider(
    std::string const& color_space, std::string const& filter) :
    width(400),
    stripe_height(80),
    color_space(color_space),
    filter(filter),
    n_stripes(6),
    j_color_space(JCS_UNKNOWN)
{
    if (color_space == "/DeviceCMYK") {
        j_color_space = JCS_CMYK;
        stripes.push_back(std::string("\xff\x00\x00\x00", 4));
        stripes.push_back(std::string("\x00\xff\x00\x00", 4));
        stripes.push_back(std::string("\x00\x00\xff\x00", 4));
        stripes.push_back(std::string("\xff\x00\xff\x00", 4));
        stripes.push_back(std::string("\xff\xff\x00\x00", 4));
        stripes.push_back(std::string("\x00\x00\x00\xff", 4));
    } else if (color_space == "/DeviceRGB") {
        j_color_space = JCS_RGB;
        stripes.push_back(std::string("\xff\x00\x00", 3));
        stripes.push_back(std::string("\x00\xff\x00", 3));
        stripes.push_back(std::string("\x00\x00\xff", 3));
        stripes.push_back(std::string("\xff\x00\xff", 3));
        stripes.push_back(std::string("\xff\xff\x00", 3));
        stripes.push_back(std::string("\x00\x00\x00", 3));
    } else if (color_space == "/DeviceGray") {
        j_color_space = JCS_GRAYSCALE;
        stripes.push_back(std::string("\xee", 1));
        stripes.push_back(std::string("\xcc", 1));
        stripes.push_back(std::string("\x99", 1));
        stripes.push_back(std::string("\x66", 1));
        stripes.push_back(std::string("\x33", 1));
        stripes.push_back(std::string("\x00", 1));
    }
}

size_t
ImageProvider::getWidth() const
{
    return width;
}

size_t
ImageProvider::getHeight() const
{
    return stripe_height * n_stripes;
}

void
ImageProvider::provideStreamData(QPDFObjGen const&, Pipeline* pipeline)
{
    std::vector<std::shared_ptr<Pipeline>> to_delete;
    Pipeline* p = pipeline;
    std::shared_ptr<Pipeline> p_new;

    if (filter == "/DCTDecode") {
        p_new = std::make_shared<Pl_DCT>(
            "image encoder",
            pipeline,
            QIntC::to_uint(width),
            QIntC::to_uint(getHeight()),
            QIntC::to_int(stripes[0].length()),
            j_color_space);
        to_delete.push_back(p_new);
        p = p_new.get();
    } else if (filter == "/RunLengthDecode") {
        p_new = std::make_shared<Pl_RunLength>(
            "image encoder", pipeline, Pl_RunLength::a_encode);
        to_delete.push_back(p_new);
        p = p_new.get();
    }

    for (size_t i = 0; i < n_stripes; ++i) {
        for (size_t j = 0; j < width * stripe_height; ++j) {
            p->writeString(stripes[i]);
        }
    }
    p->finish();
}

void
usage()
{
    std::cerr << "Usage: " << whoami << " filename" << std::endl
              << "Creates a simple PDF and writes it to filename" << std::endl;
    exit(2);
}

static QPDFObjectHandle
createPageContents(QPDF& pdf, std::string const& text)
{
    // Create a stream that displays our image and the given text in
    // our font.
    std::string contents = "BT /F1 24 Tf 72 320 Td (" + text +
        ") Tj ET\n"
        "q 244 0 0 144 184 100 cm /Im1 Do Q\n";
    return pdf.newStream(contents);
}

QPDFObjectHandle
newName(std::string const& name)
{
    return QPDFObjectHandle::newName(name);
}

QPDFObjectHandle
newInteger(size_t val)
{
    return QPDFObjectHandle::newInteger(QIntC::to_int(val));
}

void
add_page(
    QPDFPageDocumentHelper& dh,
    QPDFObjectHandle font,
    std::string const& color_space,
    std::string const& filter)
{
    QPDF& pdf(dh.getQPDF());

    // Create a stream to encode our image. QPDFWriter will fill in
    // the length and will respect our filters based on stream data
    // mode. Since we are not specifying, QPDFWriter will compress
    // with /FlateDecode if we don't provide any other form of
    // compression.
    ImageProvider* p = new ImageProvider(color_space, filter);
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider(p);
    size_t width = p->getWidth();
    size_t height = p->getHeight();
    QPDFObjectHandle image = pdf.newStream();
    auto image_dict =
        // line-break
        "<<"
        " /Type /XObject"
        " /Subtype /Image"
        " /BitsPerComponent 8"
        ">>"_qpdf;
    image_dict.replaceKey("/ColorSpace", newName(color_space));
    image_dict.replaceKey("/Width", newInteger(width));
    image_dict.replaceKey("/Height", newInteger(height));
    image.replaceDict(image_dict);

    // Provide the stream data.
    image.replaceStreamData(
        provider, QPDFObjectHandle::parse(filter), QPDFObjectHandle::newNull());

    // Create direct objects as needed by the page dictionary.
    QPDFObjectHandle procset = "[/PDF /Text /ImageC]"_qpdf;

    QPDFObjectHandle rfont = QPDFObjectHandle::newDictionary();
    rfont.replaceKey("/F1", font);

    QPDFObjectHandle xobject = QPDFObjectHandle::newDictionary();
    xobject.replaceKey("/Im1", image);

    QPDFObjectHandle resources = QPDFObjectHandle::newDictionary();
    resources.replaceKey("/ProcSet", procset);
    resources.replaceKey("/Font", rfont);
    resources.replaceKey("/XObject", xobject);

    // Create the page content stream
    QPDFObjectHandle contents =
        createPageContents(pdf, color_space + " with filter " + filter);

    // Create the page dictionary
    QPDFObjectHandle page = pdf.makeIndirectObject("<<"
                                                   " /Type /Page"
                                                   " /MediaBox [0 0 612 392]"
                                                   ">>"_qpdf);
    page.replaceKey("/Contents", contents);
    page.replaceKey("/Resources", resources);

    // Add the page to the PDF file
    dh.addPage(page, false);
}

static void
check(
    char const* filename,
    std::vector<std::string> const& color_spaces,
    std::vector<std::string> const& filters)
{
    // Each stream is compressed the way it is supposed to be. We will
    // add additional tests in qpdf.test to exercise QPDFWriter more
    // fully. In this case, we want to make sure that we actually have
    // RunLengthDecode and DCTDecode where we are supposed to and
    // FlateDecode where we provided no filters.

    // Each image is correct. For non-lossy image compression, the
    // uncompressed image data should exactly match what ImageProvider
    // provided. For the DCTDecode data, allow for some fuzz to handle
    // jpeg compression as well as its variance on different systems.

    // These tests should use QPDFObjectHandle's stream data retrieval
    // methods, but don't try to fully exercise them here. That is
    // done elsewhere.

    size_t n_color_spaces = color_spaces.size();
    size_t n_filters = filters.size();

    QPDF pdf;
    pdf.processFile(filename);
    auto pages = QPDFPageDocumentHelper(pdf).getAllPages();
    if (n_color_spaces * n_filters != pages.size()) {
        throw std::logic_error("incorrect number of pages");
    }
    size_t pageno = 1;
    bool errors = false;
    for (auto& page: pages) {
        auto images = page.getImages();
        if (images.size() != 1) {
            throw std::logic_error("incorrect number of images on page");
        }

        // Check filter and color space.
        std::string desired_color_space =
            color_spaces[(pageno - 1) / n_color_spaces];
        std::string desired_filter = filters[(pageno - 1) % n_filters];
        // In the default mode, QPDFWriter will compress with
        // /FlateDecode if no filters are provided.
        if (desired_filter == "null") {
            desired_filter = "/FlateDecode";
        }
        QPDFObjectHandle image = images.begin()->second;
        QPDFObjectHandle image_dict = image.getDict();
        QPDFObjectHandle color_space = image_dict.getKey("/ColorSpace");
        QPDFObjectHandle filter = image_dict.getKey("/Filter");
        bool this_errors = false;
        if (!filter.isNameAndEquals(desired_filter)) {
            this_errors = errors = true;
            std::cout << "page " << pageno << ": expected filter "
                      << desired_filter
                      << "; actual filter = " << filter.unparse() << std::endl;
        }
        if (!color_space.isNameAndEquals(desired_color_space)) {
            this_errors = errors = true;
            std::cout << "page " << pageno << ": expected color space "
                      << desired_color_space
                      << "; actual color space = " << color_space.unparse()
                      << std::endl;
        }

        if (!this_errors) {
            // Check image data
            auto actual_data = image.getStreamData(qpdf_dl_all);
            ImageProvider* p = new ImageProvider(desired_color_space, "null");
            std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider(p);
            Pl_Buffer b_p("get image data");
            provider->provideStreamData(QPDFObjGen(), &b_p);
            std::shared_ptr<Buffer> desired_data(b_p.getBuffer());

            if (desired_data->getSize() != actual_data->getSize()) {
                std::cout << "page " << pageno << ": image data length mismatch"
                          << std::endl;
                this_errors = errors = true;
            } else {
                // Compare bytes. For JPEG, allow a certain number of
                // the bytes to be off desired by more than a given
                // tolerance. Any of the samples may be a little off
                // because of lossy compression, and around sharp
                // edges, things can be quite off. For non-lossy
                // compression, do not allow any tolerance.
                unsigned char const* actual_bytes = actual_data->getBuffer();
                unsigned char const* desired_bytes = desired_data->getBuffer();
                size_t len = actual_data->getSize();
                unsigned int mismatches = 0;
                int tolerance = (desired_filter == "/DCTDecode" ? 10 : 0);
                size_t threshold =
                    (desired_filter == "/DCTDecode" ? len / 40U : 0);
                for (size_t i = 0; i < len; ++i) {
                    int delta = actual_bytes[i] - desired_bytes[i];
                    if ((delta > tolerance) || (delta < -tolerance)) {
                        ++mismatches;
                    }
                }
                if (mismatches > threshold) {
                    std::cout << "page " << pageno << ": "
                              << desired_color_space << ", " << desired_filter
                              << ": mismatches: " << mismatches << " of " << len
                              << std::endl;
                    this_errors = errors = true;
                }
            }
        }

        ++pageno;
    }
    if (errors) {
        throw std::logic_error("errors found");
    } else {
        std::cout << "all checks passed" << std::endl;
    }
}

static void
create_pdf(char const* filename)
{
    QPDF pdf;

    // Start with an empty PDF that has no pages or non-required objects.
    pdf.emptyPDF();

    // Add an indirect object to contain a font descriptor for the
    // built-in Helvetica font.
    QPDFObjectHandle font = pdf.makeIndirectObject(
        // line-break
        "<<"
        " /Type /Font"
        " /Subtype /Type1"
        " /Name /F1"
        " /BaseFont /Helvetica"
        " /Encoding /WinAnsiEncoding"
        ">>"_qpdf);

    std::vector<std::string> color_spaces;
    color_spaces.push_back("/DeviceCMYK");
    color_spaces.push_back("/DeviceRGB");
    color_spaces.push_back("/DeviceGray");
    std::vector<std::string> filters;
    filters.push_back("null");
    filters.push_back("/DCTDecode");
    filters.push_back("/RunLengthDecode");
    QPDFPageDocumentHelper dh(pdf);
    for (auto const& color_space: color_spaces) {
        for (auto const& filter: filters) {
            add_page(dh, font, color_space, filter);
        }
    }

    QPDFWriter w(pdf, filename);
    w.write();

    // For test suite, verify that everything is the way it is
    // supposed to be.
    check(filename, color_spaces, filters);
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 2) {
        usage();
    }
    char const* filename = argv[1];

    try {
        create_pdf(filename);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

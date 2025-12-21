// Fuzzer for page transformation operations
// Targets under-fuzzed functions like flattenRotation, copyAnnotations,
// rotatePage, removeUnreferencedResources, and form XObject operations.

#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <cstdlib>
#include <iostream>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    std::shared_ptr<QPDF> getQpdf();
    void testTransformations();
    void doChecks();

    Buffer input_buffer;
    Pl_Discard discard;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    input_buffer(const_cast<unsigned char*>(data), size)
{
}

std::shared_ptr<QPDF>
FuzzHelper::getQpdf()
{
    auto is =
        std::shared_ptr<InputSource>(new BufferInputSource("fuzz input", &this->input_buffer));
    auto qpdf = QPDF::create();
    qpdf->setMaxWarnings(200);
    qpdf->processInputSource(is);
    return qpdf;
}

void
FuzzHelper::testTransformations()
{
    std::shared_ptr<QPDF> q = getQpdf();
    QPDFPageDocumentHelper pdh(*q);
    QPDFAcroFormDocumentHelper afdh(*q);

    auto pages = pdh.getAllPages();
    if (pages.empty()) {
        return;
    }

    int pageno = 0;
    for (auto& page: pages) {
        ++pageno;
        if (pageno > 10) {
            // Limit pages processed to avoid timeouts
            break;
        }

        try {
            std::cerr << "info: processing page " << pageno << '\n';

            // Test flattenRotation - high complexity, 0% coverage
            std::cerr << "info: flattenRotation\n";
            page.flattenRotation(&afdh);

            // Test rotatePage with various angles
            std::cerr << "info: rotatePage\n";
            page.rotatePage(90, true);

            // Test removeUnreferencedResources
            std::cerr << "info: removeUnreferencedResources\n";
            page.removeUnreferencedResources();

            // Test getFormXObjectForPage
            std::cerr << "info: getFormXObjectForPage\n";
            auto form_xobj = page.getFormXObjectForPage(true);

            // Test getMatrixForTransformations
            std::cerr << "info: getMatrixForTransformations\n";
            page.getMatrixForTransformations(false);
            page.getMatrixForTransformations(true);

            // Test various box accessors
            std::cerr << "info: box accessors\n";
            page.getMediaBox(true);
            page.getCropBox(true, true);
            page.getBleedBox(true, true);
            page.getTrimBox(true, true);
            page.getArtBox(true, true);

            // Test forEachXObject, forEachImage, forEachFormXObject
            std::cerr << "info: forEach*\n";
            page.forEachXObject(
                true,
                [](QPDFObjectHandle&, QPDFObjectHandle&, std::string const&) {},
                nullptr);
            page.forEachImage(
                true, [](QPDFObjectHandle&, QPDFObjectHandle&, std::string const&) {});
            page.forEachFormXObject(
                true, [](QPDFObjectHandle&, QPDFObjectHandle&, std::string const&) {});

            // Test externalizeInlineImages
            std::cerr << "info: externalizeInlineImages\n";
            page.externalizeInlineImages(0, false);

            // Test filterContents with null filter (exercises the path)
            std::cerr << "info: pipeContents\n";
            page.pipeContents(&discard);

        } catch (QPDFExc const& e) {
            std::cerr << "page " << pageno << " QPDFExc: " << e.what() << '\n';
        } catch (std::runtime_error const& e) {
            std::cerr << "page " << pageno << " runtime_error: " << e.what() << '\n';
        }
    }

    // Test copyAnnotations between pages if we have at least 2 pages
    if (pages.size() >= 2) {
        try {
            std::cerr << "info: copyAnnotations\n";
            auto& from_page = pages[0];
            auto& to_page = pages[1];

            // Get transformation matrix
            auto matrix = from_page.getMatrixForTransformations();

            // Copy annotations from first page to second
            to_page.copyAnnotations(from_page, matrix, &afdh);
        } catch (QPDFExc const& e) {
            std::cerr << "copyAnnotations QPDFExc: " << e.what() << '\n';
        } catch (std::runtime_error const& e) {
            std::cerr << "copyAnnotations runtime_error: " << e.what() << '\n';
        }
    }

    // Test AcroForm operations
    try {
        std::cerr << "info: afdh.validate\n";
        afdh.validate(true);

        std::cerr << "info: getFormFields\n";
        auto fields = afdh.getFormFields();

        std::cerr << "info: hasAcroForm\n";
        afdh.hasAcroForm();

        std::cerr << "info: getNeedAppearances\n";
        afdh.getNeedAppearances();

    } catch (QPDFExc const& e) {
        std::cerr << "afdh QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "afdh runtime_error: " << e.what() << '\n';
    }

    // Write output to exercise writer with transformed document
    try {
        std::cerr << "info: writing output\n";
        QPDFWriter w(*q);
        w.setOutputPipeline(&discard);
        w.setDeterministicID(true);
        w.write();
    } catch (QPDFExc const& e) {
        std::cerr << "write QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "write runtime_error: " << e.what() << '\n';
    }
}

void
FuzzHelper::doChecks()
{
    Pl_DCT::setMemoryLimit(100'000'000);
    Pl_DCT::setScanLimit(50);
    Pl_PNGFilter::setMemoryLimit(1'000'000);
    Pl_RunLength::setMemoryLimit(1'000'000);
    Pl_TIFFPredictor::setMemoryLimit(1'000'000);
    Pl_Flate::memory_limit(200'000);
    Pl_DCT::setThrowOnCorruptData(true);

    std::cerr << "\ninfo: starting testTransformations\n";
    testTransformations();
}

void
FuzzHelper::run()
{
    try {
        doChecks();
    } catch (QPDFExc const& e) {
        std::cerr << "QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "runtime_error: " << e.what() << '\n';
    }
}

extern "C" int
LLVMFuzzerTestOneInput(unsigned char const* data, size_t size)
{
#ifndef _WIN32
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

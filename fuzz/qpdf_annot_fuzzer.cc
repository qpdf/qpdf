// Fuzzer for annotation handling operations
// Targets transformAnnotations, fixCopiedAnnotations, and annotation helpers

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
#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    std::shared_ptr<QPDF> getQpdf();
    void testAnnotations();
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
FuzzHelper::testAnnotations()
{
    std::shared_ptr<QPDF> q = getQpdf();
    QPDFPageDocumentHelper pdh(*q);
    QPDFAcroFormDocumentHelper afdh(*q);

    auto pages = pdh.getAllPages();
    if (pages.empty()) {
        return;
    }

    // Test annotation operations on each page
    int pageno = 0;
    for (auto& page: pages) {
        ++pageno;
        if (pageno > 5) {
            break; // Limit pages to avoid timeouts
        }

        try {
            std::cerr << "info: processing page " << pageno << '\n';

            // Get all annotations on the page
            std::cerr << "info: getAnnotations (all)\n";
            auto all_annots = page.getAnnotations();

            // Get specific annotation types
            std::cerr << "info: getAnnotations (Link)\n";
            page.getAnnotations("Link");

            std::cerr << "info: getAnnotations (Widget)\n";
            auto widget_annots = page.getAnnotations("Widget");

            std::cerr << "info: getAnnotations (Text)\n";
            page.getAnnotations("Text");

            // Test annotation object helper on each annotation
            for (auto& annot: all_annots) {
                std::cerr << "info: testing annotation helper\n";

                // Get annotation properties
                annot.getSubtype();
                annot.getAppearanceState();
                annot.getAppearanceDictionary();

                // Get flags
                annot.getFlags();
            }

            // Test widget annotations with form helper
            for (auto& widget: widget_annots) {
                std::cerr << "info: getFieldForAnnotation\n";
                auto field = afdh.getFieldForAnnotation(widget);
            }

        } catch (QPDFExc const& e) {
            std::cerr << "page " << pageno << " QPDFExc: " << e.what() << '\n';
        } catch (std::runtime_error const& e) {
            std::cerr << "page " << pageno << " runtime_error: " << e.what() << '\n';
        }
    }

    // Test transformAnnotations if we have at least 2 pages
    if (pages.size() >= 2) {
        try {
            std::cerr << "info: transformAnnotations\n";

            auto& from_page = pages[0];
            auto& to_page = pages[1];

            std::vector<QPDFObjectHandle> new_annots;
            std::vector<QPDFObjectHandle> new_fields;
            std::set<QPDFObjGen> old_fields;

            // Get transformation matrix
            auto matrix = from_page.getMatrixForTransformations();

            // Transform annotations from one page to another
            afdh.transformAnnotations(
                from_page.getObjectHandle(),
                new_annots,
                new_fields,
                old_fields,
                matrix);

            std::cerr << "info: transformAnnotations produced " << new_annots.size()
                      << " annotations\n";

            // Test fixCopiedAnnotations
            if (!new_annots.empty()) {
                std::cerr << "info: fixCopiedAnnotations\n";
                std::set<QPDFObjGen> fields_set;
                afdh.fixCopiedAnnotations(
                    to_page.getObjectHandle(), from_page.getObjectHandle(), afdh, &fields_set);
            }

        } catch (QPDFExc const& e) {
            std::cerr << "transform QPDFExc: " << e.what() << '\n';
        } catch (std::runtime_error const& e) {
            std::cerr << "transform runtime_error: " << e.what() << '\n';
        }
    }

    // Test AcroForm validation and operations
    try {
        std::cerr << "info: afdh operations\n";

        // Validate form structure
        afdh.validate(true);

        // Get form fields
        auto fields = afdh.getFormFields();
        std::cerr << "info: found " << fields.size() << " form fields\n";

        // Test each form field
        int field_count = 0;
        for (auto& field: fields) {
            ++field_count;
            if (field_count > 10) {
                break;
            }

            // Get field properties
            field.getFieldType();
            field.getFullyQualifiedName();
            field.getPartialName();
            field.getValue();
            field.getDefaultValue();
            field.getValueAsString();
            field.getDefaultValueAsString();
            field.isText();
            field.isCheckbox();
            field.isRadioButton();
            field.isChoice();

            // Get annotations for field
            afdh.getAnnotationsForField(field);
        }

        // Test need appearances
        afdh.getNeedAppearances();
        afdh.setNeedAppearances(false);

        // Disable digital signatures (exercises signature handling code)
        std::cerr << "info: disableDigitalSignatures\n";
        afdh.disableDigitalSignatures();

    } catch (QPDFExc const& e) {
        std::cerr << "afdh QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "afdh runtime_error: " << e.what() << '\n';
    }

    // Write output
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

    std::cerr << "\ninfo: starting testAnnotations\n";
    testAnnotations();
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

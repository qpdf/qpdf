#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/global.hh>

#include <chrono>
#include <iostream>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size) :
        // We do not modify data, so it is safe to remove the const for Buffer
        input_buffer(const_cast<unsigned char*>(data), size)
    {
    }

    void
    run()
    {
        qpdf::global::options::fuzz_mode(true);
        // The goal here is that you should be able to throw anything at libqpdf and it will respond
        // without any memory errors and never do anything worse than throwing a QPDFExc or
        // std::runtime_error. Throwing any other kind of exception, segfaulting, or having a memory
        // error (when built with appropriate sanitizers) will all cause abnormal exit.
        try {
            info("");
            // Write in various ways to exercise QPDFWriter

            auto is = std::make_shared<BufferInputSource>("fuzz input", &input_buffer);
            QPDF qpdf;
            qpdf.processInputSource(is);
            info("processInputSource done");
            QPDFPageDocumentHelper pdh(qpdf);
            QPDFPageLabelDocumentHelper pldh(qpdf);
            QPDFOutlineDocumentHelper odh(qpdf);
            QPDFAcroFormDocumentHelper afdh(qpdf);
            afdh.generateAppearancesIfNeeded();
            info("generateAppearancesIfNeeded done");
            pdh.flattenAnnotations();
            info("flattenAnnotations done");
            int pageno = 0;
            for (auto& page: pdh.getAllPages()) {
                ++pageno;
                try {
                    info("start page", pageno);
                    page.coalesceContentStreams();
                    info("coalesceContentStreams done");
                    page.parseContents(nullptr);
                    info("parseContents done");
                    page.getImages();
                    info("getImages done");
                    pldh.getLabelForPage(pageno);
                    info("getLabelForPage done");
                    QPDFObjectHandle page_obj(page.getObjectHandle());
                    page_obj.getJSON(JSON::LATEST, true).unparse();
                    info("getJSON done");
                    odh.getOutlinesForPage(page_obj);
                    info("getOutlinesForPage done");

                    for (auto& aoh: afdh.getWidgetAnnotationsForPage(page)) {
                        afdh.getFieldForAnnotation(aoh);
                    }
                } catch (QPDFExc& e) {
                    std::cerr << "page " << pageno << ": " << e.what() << '\n';
                }
            }
        } catch (std::runtime_error const& e) {
            std::cerr << "runtime_error: " << e.what() << '\n';
        }
    }

  private:
    void
    info(std::string const& msg, int pageno = 0) const
    {
        const std::chrono::duration<double> elapsed{std::chrono::steady_clock::now() - start};

        std::cerr << elapsed.count() << " info - " << msg;
        if (pageno > 0) {
            std::cerr << " page " << pageno;
        }
        std::cerr << '\n';
    }

    Buffer input_buffer;
    Pl_Discard discard;
    const std::chrono::time_point<std::chrono::steady_clock> start;
};

extern "C" int
LLVMFuzzerTestOneInput(unsigned char const* data, size_t size)
{
#ifndef _WIN32
    // Used by jpeg library to work around false positives in memory sanitizer.
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

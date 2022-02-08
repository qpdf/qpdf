#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <cstdlib>

class DiscardContents: public QPDFObjectHandle::ParserCallbacks
{
  public:
    virtual ~DiscardContents() {}
    virtual void handleObject(QPDFObjectHandle) {}
    virtual void handleEOF() {}
};

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    PointerHolder<QPDF> getQpdf();
    PointerHolder<QPDFWriter> getWriter(PointerHolder<QPDF>);
    void doWrite(PointerHolder<QPDFWriter> w);
    void testWrite();
    void testPages();
    void testOutlines();
    void doChecks();

    Buffer input_buffer;
    Pl_Discard discard;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    // We do not modify data, so it is safe to remove the const for Buffer
    input_buffer(const_cast<unsigned char*>(data), size)
{
}

PointerHolder<QPDF>
FuzzHelper::getQpdf()
{
    auto is = PointerHolder<InputSource>(
        new BufferInputSource("fuzz input", &this->input_buffer));
    auto qpdf = make_pointer_holder<QPDF>();
    qpdf->processInputSource(is);
    return qpdf;
}

PointerHolder<QPDFWriter>
FuzzHelper::getWriter(PointerHolder<QPDF> qpdf)
{
    auto w = make_pointer_holder<QPDFWriter>(*qpdf);
    w->setOutputPipeline(&this->discard);
    w->setDecodeLevel(qpdf_dl_all);
    return w;
}

void
FuzzHelper::doWrite(PointerHolder<QPDFWriter> w)
{
    try
    {
        w->write();
    }
    catch (QPDFExc const& e)
    {
        std::cerr << e.what() << std::endl;
    }
    catch (std::runtime_error const& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

void
FuzzHelper::testWrite()
{
    // Write in various ways to exercise QPDFWriter

    PointerHolder<QPDF> q;
    PointerHolder<QPDFWriter> w;

    q = getQpdf();
    w = getWriter(q);
    w->setDeterministicID(true);
    w->setQDFMode(true);
    doWrite(w);

    q = getQpdf();
    w = getWriter(q);
    w->setStaticID(true);
    w->setLinearization(true);
    w->setR6EncryptionParameters(
        "u", "o", true, true, true, true, true, true, qpdf_r3p_full, true);
    doWrite(w);

    q = getQpdf();
    w = getWriter(q);
    w->setStaticID(true);
    w->setObjectStreamMode(qpdf_o_disable);
    w->setR3EncryptionParameters(
        "u", "o", true, true, true, true, true, true, qpdf_r3p_full);
    doWrite(w);

    q = getQpdf();
    w = getWriter(q);
    w->setDeterministicID(true);
    w->setObjectStreamMode(qpdf_o_generate);
    w->setLinearization(true);
    doWrite(w);
}

void
FuzzHelper::testPages()
{
    // Parse all content streams, and exercise some helpers that
    // operate on pages.
    PointerHolder<QPDF> q = getQpdf();
    QPDFPageDocumentHelper pdh(*q);
    QPDFPageLabelDocumentHelper pldh(*q);
    QPDFOutlineDocumentHelper odh(*q);
    QPDFAcroFormDocumentHelper afdh(*q);
    afdh.generateAppearancesIfNeeded();
    pdh.flattenAnnotations();
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    DiscardContents discard_contents;
    int pageno = 0;
    for (std::vector<QPDFPageObjectHelper>::iterator iter =
             pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFPageObjectHelper& page(*iter);
        ++pageno;
        try
        {
            page.coalesceContentStreams();
            page.parseContents(&discard_contents);
            page.getImages();
            pldh.getLabelForPage(pageno);
            QPDFObjectHandle page_obj(page.getObjectHandle());
            page_obj.getJSON(true).unparse();
            odh.getOutlinesForPage(page_obj.getObjGen());

            std::vector<QPDFAnnotationObjectHelper> annotations =
                afdh.getWidgetAnnotationsForPage(page);
            for (std::vector<QPDFAnnotationObjectHelper>::iterator annot_iter =
                     annotations.begin();
                 annot_iter != annotations.end(); ++annot_iter)
            {
                QPDFAnnotationObjectHelper& aoh = *annot_iter;
                afdh.getFieldForAnnotation(aoh);
            }
        }
        catch (QPDFExc& e)
        {
            std::cerr << "page " << pageno << ": "
                      << e.what() << std::endl;
        }
    }
}

void
FuzzHelper::testOutlines()
{
    PointerHolder<QPDF> q = getQpdf();
    std::list<std::vector<QPDFOutlineObjectHelper> > queue;
    QPDFOutlineDocumentHelper odh(*q);
    queue.push_back(odh.getTopLevelOutlines());
    while (! queue.empty())
    {
        std::vector<QPDFOutlineObjectHelper>& outlines = *(queue.begin());
        for (std::vector<QPDFOutlineObjectHelper>::iterator iter =
                 outlines.begin();
             iter != outlines.end(); ++iter)
        {
            QPDFOutlineObjectHelper& ol = *iter;
            ol.getDestPage();
            queue.push_back(ol.getKids());
        }
        queue.pop_front();
    }
}

void
FuzzHelper::doChecks()
{
    // Get as much coverage as possible in parts of the library that
    // might benefit from fuzzing.
    testWrite();
    testPages();
    testOutlines();
}

void
FuzzHelper::run()
{
    // The goal here is that you should be able to throw anything at
    // libqpdf and it will respond without any memory errors and never
    // do anything worse than throwing a QPDFExc or
    // std::runtime_error. Throwing any other kind of exception,
    // segfaulting, or having a memory error (when built with
    // appropriate sanitizers) will all cause abnormal exit.
    try
    {
        doChecks();
    }
    catch (QPDFExc const& e)
    {
        std::cerr << "QPDFExc: " << e.what() << std::endl;
    }
    catch (std::runtime_error const& e)
    {
        std::cerr << "runtime_error: " << e.what() << std::endl;
    }
}

extern "C" int LLVMFuzzerTestOneInput(unsigned char const* data, size_t size)
{
#ifndef _WIN32
    // Used by jpeg library to work around false positives in memory
    // sanitizer.
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

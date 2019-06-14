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
    PointerHolder<InputSource> is =
        new BufferInputSource("fuzz input", &this->input_buffer);
    PointerHolder<QPDF> qpdf = new QPDF();
    qpdf->processInputSource(is);
    return qpdf;
}

PointerHolder<QPDFWriter>
FuzzHelper::getWriter(PointerHolder<QPDF> qpdf)
{
    PointerHolder<QPDFWriter> w = new QPDFWriter(*qpdf);
    w->setOutputPipeline(&this->discard);
    w->setDeterministicID(true);
    w->setDecodeLevel(qpdf_dl_all);
    w->setCompressStreams(false);
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
}

void
FuzzHelper::testWrite()
{
    // Write in various ways to exercise QPDFWriter

    PointerHolder<QPDF> q;
    PointerHolder<QPDFWriter> w;

    q = getQpdf();
    w = getWriter(q);
    doWrite(w);

    q = getQpdf();
    w = getWriter(q);
    w->setLinearization(true);
    doWrite(w);

    q = getQpdf();
    w = getWriter(q);
    w->setObjectStreamMode(qpdf_o_disable);
    doWrite(w);

    q = getQpdf();
    w = getWriter(q);
    w->setObjectStreamMode(qpdf_o_generate);
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
            page.parsePageContents(&discard_contents);
            page.getPageImages();
            pldh.getLabelForPage(pageno);
            odh.getOutlinesForPage(page.getObjectHandle().getObjGen());

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
    std::list<std::list<QPDFOutlineObjectHelper> > queue;
    QPDFOutlineDocumentHelper odh(*q);
    queue.push_back(odh.getTopLevelOutlines());
    while (! queue.empty())
    {
        std::list<QPDFOutlineObjectHelper>& outlines = *(queue.begin());
        for (std::list<QPDFOutlineObjectHelper>::iterator iter =
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
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

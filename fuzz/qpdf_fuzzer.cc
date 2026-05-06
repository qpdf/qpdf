#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/global.hh>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    std::shared_ptr<QPDF> getQpdf();
    std::unique_ptr<QPDFWriter> getWriter(std::shared_ptr<QPDF>);
    void doWrite(QPDFWriter& w);
    void testWrite();
    void doChecks();

    Buffer input_buffer;
    Pl_Discard discard;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    // We do not modify data, so it is safe to remove the const for Buffer
    input_buffer(const_cast<unsigned char*>(data), size)
{
}

std::shared_ptr<QPDF>
FuzzHelper::getQpdf()
{
    auto is = std::make_shared<BufferInputSource>("fuzz input", &this->input_buffer);
    auto qpdf = QPDF::create();
    qpdf->processInputSource(is);
    return qpdf;
}

std::unique_ptr<QPDFWriter>
FuzzHelper::getWriter(std::shared_ptr<QPDF> qpdf)
{
    auto w = std::make_unique<QPDFWriter>(*qpdf);
    w->setOutputPipeline(&discard);
    w->setDecodeLevel(qpdf_dl_all);
    return w;
}

void
FuzzHelper::doWrite(QPDFWriter& w)
{
    try {
        w.write();
    } catch (QPDFExc const& e) {
        std::cerr << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << e.what() << '\n';
    }
}

void
FuzzHelper::testWrite()
{
    // Write in various ways to exercise QPDFWriter

    std::shared_ptr<QPDF> q;

    q = getQpdf();
    auto w = getWriter(q);
    w->setDeterministicID(true);
    w->setQDFMode(true);
    doWrite(*w);
}

void
FuzzHelper::doChecks()
{
    qpdf::global::options::fuzz_mode(true);
    std::cerr << "\ninfo: starting testWrite\n";
    testWrite();
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
    // Used by jpeg library to work around false positives in memory
    // sanitizer.
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

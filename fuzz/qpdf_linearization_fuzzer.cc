// Fuzzer for linearization operations
// Targets isLinearized, checkLinearization, showLinearizationData

#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
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
    void testLinearization();
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
FuzzHelper::testLinearization()
{
    std::shared_ptr<QPDF> q = getQpdf();

    // Test linearization checks
    std::cerr << "info: isLinearized\n";
    bool is_linearized = q->isLinearized();

    std::cerr << "info: checkLinearization\n";
    q->checkLinearization();

    if (is_linearized) {
        // Note: showLinearizationData() outputs to cout which creates noise,
        // so we skip calling it. checkLinearization() exercises similar code paths.
        std::cerr << "info: PDF is linearized, skipping showLinearizationData\n";
    }

    // Test writing with linearization enabled
    try {
        std::cerr << "info: write with linearization\n";
        QPDFWriter w(*q);
        w.setOutputPipeline(&discard);
        w.setDeterministicID(true);
        w.setLinearization(true);
        w.write();
    } catch (QPDFExc const& e) {
        std::cerr << "linearize write QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "linearize write runtime_error: " << e.what() << '\n';
    }

    // Test writing with object streams
    try {
        std::cerr << "info: write with object streams\n";
        QPDFWriter w(*q);
        w.setOutputPipeline(&discard);
        w.setDeterministicID(true);
        w.setObjectStreamMode(qpdf_o_generate);
        w.write();
    } catch (QPDFExc const& e) {
        std::cerr << "objstream write QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "objstream write runtime_error: " << e.what() << '\n';
    }

    // Test linearization + object streams combined
    try {
        std::cerr << "info: write with linearization + object streams\n";
        QPDFWriter w(*q);
        w.setOutputPipeline(&discard);
        w.setDeterministicID(true);
        w.setLinearization(true);
        w.setObjectStreamMode(qpdf_o_generate);
        w.write();
    } catch (QPDFExc const& e) {
        std::cerr << "combined write QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "combined write runtime_error: " << e.what() << '\n';
    }

    // Test re-linearizing: write to buffer and re-process
    try {
        std::cerr << "info: re-linearization test\n";
        Pl_Buffer pb("linearized output");
        {
            QPDFWriter w(*q);
            w.setOutputPipeline(&pb);
            w.setDeterministicID(true);
            w.setLinearization(true);
            w.write();
        }

        // Get the linearized output
        auto buf = pb.getBufferSharedPointer();
        if (buf && buf->getSize() > 0) {
            // Re-parse the linearized PDF
            auto is2 = std::make_shared<BufferInputSource>("linearized", buf.get());
            auto q2 = QPDF::create();
            q2->setMaxWarnings(200);
            q2->processInputSource(is2);

            // Check the re-linearized version
            std::cerr << "info: verify re-linearized\n";
            q2->isLinearized();
            q2->checkLinearization();
        }
    } catch (QPDFExc const& e) {
        std::cerr << "relinearize QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "relinearize runtime_error: " << e.what() << '\n';
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

    std::cerr << "\ninfo: starting testLinearization\n";
    testLinearization();
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

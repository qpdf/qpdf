#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/global.hh>

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
            std::cerr << "\ninfo: starting testWrite\n";

            auto is = std::make_shared<BufferInputSource>("fuzz input", &input_buffer);
            QPDF qpdf;
            qpdf.processInputSource(is);
            QPDFWriter w(qpdf);
            w.setOutputPipeline(&discard);
            w.setDecodeLevel(qpdf_dl_all);
            w.setDeterministicID(true);
            w.setObjectStreamMode(qpdf_o_generate);
            w.setLinearization(true);
            w.write();
        } catch (std::runtime_error const& e) {
            std::cerr << "runtime_error: " << e.what() << '\n';
        }
    }

  private:
    Buffer input_buffer;
    Pl_Discard discard;
};

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

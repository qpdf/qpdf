// Fuzzer for QPDFJob JSON configuration parsing
// This exercises the JSON job configuration interface which is a complex
// attack surface for parsing untrusted configuration.

#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFUsage.hh>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    void testJobJson();
    void doChecks();

    std::string json_input;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    json_input(reinterpret_cast<char const*>(data), size)
{
}

void
FuzzHelper::testJobJson()
{
    // Create a QPDFJob and attempt to initialize it from JSON
    // The JSON may be malformed or have invalid options
    QPDFJob job;

    // Suppress all output from the job
    auto logger = QPDFLogger::create();
    std::ostringstream null_stream;
    logger->setInfo(std::make_shared<Pl_Discard>());
    logger->setWarn(std::make_shared<Pl_Discard>());
    logger->setError(std::make_shared<Pl_Discard>());
    job.setLogger(logger);

    // Try parsing as partial JSON (more lenient)
    try {
        job.initializeFromJson(json_input, true);
    } catch (QPDFUsage const& e) {
        std::cerr << "QPDFUsage (partial): " << e.what() << '\n';
    } catch (std::exception const& e) {
        std::cerr << "exception (partial): " << e.what() << '\n';
    }

    // Try parsing as full JSON (stricter validation)
    QPDFJob job2;
    job2.setLogger(logger);
    try {
        job2.initializeFromJson(json_input, false);
        // If parsing succeeds, try to check configuration
        job2.checkConfiguration();
    } catch (QPDFUsage const& e) {
        std::cerr << "QPDFUsage (full): " << e.what() << '\n';
    } catch (std::exception const& e) {
        std::cerr << "exception (full): " << e.what() << '\n';
    }
}

void
FuzzHelper::doChecks()
{
    // Set memory limits for any decompression that might happen
    Pl_DCT::setMemoryLimit(100'000'000);
    Pl_DCT::setScanLimit(50);
    Pl_PNGFilter::setMemoryLimit(1'000'000);
    Pl_RunLength::setMemoryLimit(1'000'000);
    Pl_TIFFPredictor::setMemoryLimit(1'000'000);
    Pl_Flate::memory_limit(200'000);
    Pl_DCT::setThrowOnCorruptData(true);

    std::cerr << "\ninfo: starting testJobJson\n";
    testJobJson();
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
    // Limit input size to prevent excessive processing time
    static constexpr size_t kMaxInputSize = 64 * 1024; // 64KB
    if (size == 0 || size > kMaxInputSize) {
        return 0;
    }

#ifndef _WIN32
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

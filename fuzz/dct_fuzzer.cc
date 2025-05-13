#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    void doChecks();

    unsigned char const* data;
    size_t size;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    data(data),
    size(size)
{
}

void
FuzzHelper::doChecks()
{
    // Limit the memory used to decompress JPEG files during fuzzing. Excessive memory use during
    // fuzzing is due to corrupt JPEG data which sometimes cannot be detected before
    // jpeg_start_decompress is called. During normal use of qpdf very large JPEGs can occasionally
    // occur legitimately and therefore must be allowed during normal operations.
    Pl_DCT::setMemoryLimit(200'000'000);
    Pl_DCT::setScanLimit(50);

    // Do not decompress corrupt data. This may cause extended runtime within jpeglib without
    // exercising additional code paths in qpdf.
    Pl_DCT::setThrowOnCorruptData(true);

    Pl_Discard discard;
    Pl_DCT p("decode", &discard);
    p.write(const_cast<unsigned char*>(data), size);
    p.finish();
}

void
FuzzHelper::run()
{
    try {
        doChecks();
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

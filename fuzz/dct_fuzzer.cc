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

class DecompressConfig: public Pl_DCT::DecompressConfig {
    public:
    void apply(jpeg_decompress_struct *) override;
};

void DecompressConfig::apply(jpeg_decompress_struct* cinfo)
{
    // Limit the memory used to decompress JPEG files during fuzzing. Excessive memory use during
    // fuzzing is due to corrupt JPEG data which sometimes cannot be detected before
    // jpeg_start_decompress is called. During normal use of qpdf very large JPEGs can occasionally
    // occur legitimately and therefore must be allowed during normal operations.
    cinfo->mem->max_memory_to_use = 1'000'000'000;

}

void
FuzzHelper::doChecks()
{
    Pl_Discard discard;
    DecompressConfig dc;
    Pl_DCT p("decode", &discard, &dc);
    // For some corrupt files the memory used internally by libjpeg stays within the above limits
    // even though the size written to the next pipeline is significantly larger.
    p.setCorruptDataLimit(10'000'000);
    p.write(const_cast<unsigned char*>(data), size);
    p.finish();
}

void
FuzzHelper::run()
{
    try {
        doChecks();
    } catch (std::runtime_error const& e) {
        std::cerr << "runtime_error: " << e.what() << std::endl;
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

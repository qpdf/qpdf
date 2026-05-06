#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/global.hh>

#include <iostream>
#include <stdexcept>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size) :
        data(data),
        size(size)
    {
    }

    void
    run()
    {
        qpdf::global::options::fuzz_mode(true);

        Pl_Discard discard;
        Pl_DCT p("decode", &discard);
        try {
            p.write(data, size);
            p.finish();
        } catch (std::runtime_error const& e) {
            std::cerr << "runtime_error: " << e.what() << '\n';
        }
    }

  private:
    unsigned char const* data;
    size_t size;
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

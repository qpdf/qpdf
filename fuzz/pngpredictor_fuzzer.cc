#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_PNGFilter.hh>
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
        Pl_Discard discard;
        Pl_PNGFilter p("decode", &discard, Pl_PNGFilter::a_decode, 32, 1, 8);
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
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

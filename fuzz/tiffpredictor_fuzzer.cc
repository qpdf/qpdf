#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
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
        Pl_TIFFPredictor p("decoder", &discard, Pl_TIFFPredictor::a_decode, 16, 1, 8);
        // Exercise with strange values for some of the parameters.
        Pl_TIFFPredictor p2("decoder", &discard, Pl_TIFFPredictor::a_decode, 16, 2, 5);
        try {
            p.write(data, size);
            p.finish();
            p2.write(data, size);
            p2.finish();
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

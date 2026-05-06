#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_RunLength.hh>
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
        Pl_RunLength p("decode", &discard, Pl_RunLength::a_decode);
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

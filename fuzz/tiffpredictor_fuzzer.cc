#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
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
    Pl_Discard discard;
    Pl_TIFFPredictor p("decoder", &discard, Pl_TIFFPredictor::a_decode, 16, 1, 8);
    p.write(const_cast<unsigned char*>(data), size);
    p.finish();
    // Exercise with strange values for some of the parameters.
    Pl_TIFFPredictor p2("decoder", &discard, Pl_TIFFPredictor::a_decode, 16, 2, 5);
    p2.write(const_cast<unsigned char*>(data), size);
    p2.finish();
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
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

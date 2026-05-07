#include "qpdf/JSON.hh"
#include "qpdf/QPDF.hh"
#include <qpdf/BufferInputSource.hh>
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
        try {
            JSON::parse(std::string(reinterpret_cast<char const*>(data), size));
        } catch (std::runtime_error& e) {
            std::cerr << "runtime_error parsing json: " << e.what() << '\n';
        }

        QPDF q;
        Buffer buf(const_cast<unsigned char*>(data), size);
        auto is = std::make_shared<BufferInputSource>("json", &buf);
        try {
            q.createFromJSON(is);
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

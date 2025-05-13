#include "qpdf/JSON.hh"
#include "qpdf/QPDF.hh"
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Discard.hh>
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
    try {
        JSON::parse(std::string(reinterpret_cast<char const*>(data), size));
    } catch (std::runtime_error& e) {
        std::cerr << "runtime_error parsing json: " << e.what() << '\n';
    }
    QPDF q;
    q.setMaxWarnings(1000);
    Buffer buf(const_cast<unsigned char*>(data), size);
    auto is = std::make_shared<BufferInputSource>("json", &buf);
    q.createFromJSON(is);
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
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

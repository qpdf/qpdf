#include <qpdf/Pl_OStream.hh>

#include <stdexcept>

class Pl_OStream::Members
{
  public:
    Members(std::ostream& os) :
        os(os)
    {
    }
    Members(Members const&) = delete;
    ~Members() = default;

    std::ostream& os;
};

Pl_OStream::Pl_OStream(char const* identifier, std::ostream& os) :
    Pipeline(identifier, nullptr),
    m(std::make_unique<Members>(os))
{
}

// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
Pl_OStream::~Pl_OStream() = default;

void
Pl_OStream::write(unsigned char const* buf, size_t len)
{
    m->os.write(reinterpret_cast<char const*>(buf), static_cast<std::streamsize>(len));
}

void
Pl_OStream::finish()
{
    m->os.flush();
}

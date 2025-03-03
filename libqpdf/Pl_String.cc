#include <qpdf/Pl_String.hh>

#include <stdexcept>

class Pl_String::Members
{
  public:
    Members(std::string& s) :
        s(s)
    {
    }
    Members(Members const&) = delete;
    ~Members() = default;

    std::string& s;
};

Pl_String::Pl_String(char const* identifier, Pipeline* next, std::string& s) :
    Pipeline(identifier, next),
    m(std::make_unique<Members>(s))
{
}

// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
Pl_String::~Pl_String() = default;

void
Pl_String::write(unsigned char const* buf, size_t len)
{
    if (!len) {
        return;
    }
    m->s.append(reinterpret_cast<char const*>(buf), len);
    if (next()) {
        next()->write(buf, len);
    }
}

void
Pl_String::finish()
{
    if (next()) {
        next()->finish();
    }
}

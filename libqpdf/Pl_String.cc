#include <qpdf/Pl_String.hh>

#include <qpdf/QUtil.hh>
#include <errno.h>
#include <stdexcept>

Pl_String::Members::Members(std::string& s) :
    s(s)
{
}

Pl_String::Pl_String(char const* identifier, std::string& s) :
    Pipeline(identifier, 0),
    m(new Members(s))
{
}

Pl_String::~Pl_String()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_String::write(unsigned char const* buf, size_t len)
{
    this->m->s.append(reinterpret_cast<char const*>(buf), len);
}

void
Pl_String::finish()
{
}

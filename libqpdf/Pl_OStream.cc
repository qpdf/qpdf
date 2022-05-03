#include <qpdf/Pl_OStream.hh>

#include <qpdf/QUtil.hh>
#include <errno.h>
#include <stdexcept>

Pl_OStream::Members::Members(std::ostream& os) :
    os(os)
{
}

Pl_OStream::Pl_OStream(char const* identifier, std::ostream& os) :
    Pipeline(identifier, 0),
    m(new Members(os))
{
}

Pl_OStream::~Pl_OStream()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_OStream::write(unsigned char const* buf, size_t len)
{
    this->m->os.write(
        reinterpret_cast<char const*>(buf), static_cast<std::streamsize>(len));
}

void
Pl_OStream::finish()
{
    this->m->os.flush();
}

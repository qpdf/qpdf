#include <qpdf/Pl_Function.hh>

#include <qpdf/QUtil.hh>
#include <errno.h>
#include <stdexcept>

Pl_Function::Members::Members(writer_t fn) :
    fn(fn)
{
}

Pl_Function::Pl_Function(char const* identifier, Pipeline* next, writer_t fn) :
    Pipeline(identifier, next),
    m(new Members(fn))
{
}

Pl_Function::~Pl_Function()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_Function::write(unsigned char const* buf, size_t len)
{
    this->m->fn(buf, len);
    if (getNext(true)) {
        getNext()->write(buf, len);
    }
}

void
Pl_Function::finish()
{
    if (getNext(true)) {
        getNext()->finish();
    }
}

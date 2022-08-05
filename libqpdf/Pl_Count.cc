#include <qpdf/Pl_Count.hh>

#include <qpdf/QIntC.hh>

Pl_Count::Members::Members() :
    count(0),
    last_char('\0')
{
}

Pl_Count::Pl_Count(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    m(new Members())
{
}

Pl_Count::~Pl_Count()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_Count::write(unsigned char const* buf, size_t len)
{
    if (len != 0u) {
        this->m->count += QIntC::to_offset(len);
        this->m->last_char = buf[len - 1];
        getNext()->write(buf, len);
    }
}

void
Pl_Count::finish()
{
    getNext()->finish();
}

qpdf_offset_t
Pl_Count::getCount() const
{
    return this->m->count;
}

unsigned char
Pl_Count::getLastChar() const
{
    return this->m->last_char;
}

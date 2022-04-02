#include <qpdf/Pl_Count.hh>

#include <qpdf/QIntC.hh>

Pl_Count::Members::Members() :
    count(0),
    last_char('\0')
{
}

Pl_Count::Members::~Members()
{
}

Pl_Count::Pl_Count(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    m(new Members())
{
}

Pl_Count::~Pl_Count()
{
}

void
Pl_Count::write(unsigned char* buf, size_t len)
{
    if (len) {
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

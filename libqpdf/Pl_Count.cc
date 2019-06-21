#include <qpdf/Pl_Count.hh>
#include <qpdf/QIntC.hh>

Pl_Count::Pl_Count(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    count(0),
    last_char('\0')
{
}

Pl_Count::~Pl_Count()
{
}

void
Pl_Count::write(unsigned char* buf, size_t len)
{
    if (len)
    {
	this->count += QIntC::to_offset(len);
	getNext()->write(buf, len);
	this->last_char = buf[len - 1];
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
    return this->count;
}

unsigned char
Pl_Count::getLastChar() const
{
    return this->last_char;
}

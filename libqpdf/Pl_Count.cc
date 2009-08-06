
#include <qpdf/Pl_Count.hh>

DLL_EXPORT
Pl_Count::Pl_Count(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    count(0),
    last_char('\0')
{
}

DLL_EXPORT
Pl_Count::~Pl_Count()
{
}

DLL_EXPORT
void
Pl_Count::write(unsigned char* buf, int len)
{
    if (len)
    {
	this->count += len;
	getNext()->write(buf, len);
	this->last_char = buf[len - 1];
    }
}

DLL_EXPORT
void
Pl_Count::finish()
{
    getNext()->finish();
}

DLL_EXPORT
int
Pl_Count::getCount() const
{
    return this->count;
}

DLL_EXPORT
unsigned char
Pl_Count::getLastChar() const
{
    return this->last_char;
}

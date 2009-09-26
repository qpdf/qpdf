#include <qpdf/Buffer.hh>

#include <string.h>

DLL_EXPORT
Buffer::Buffer()
{
    init(0);
}

DLL_EXPORT
Buffer::Buffer(unsigned long size)
{
    init(size);
}

DLL_EXPORT
Buffer::Buffer(Buffer const& rhs)
{
    init(0);
    copy(rhs);
}

DLL_EXPORT
Buffer&
Buffer::operator=(Buffer const& rhs)
{
    copy(rhs);
    return *this;
}

DLL_EXPORT
Buffer::~Buffer()
{
    destroy();
}

void
Buffer::init(unsigned long size)
{
    this->size = size;
    this->buf = (size ? new unsigned char[size] : 0);
}

void
Buffer::copy(Buffer const& rhs)
{
    if (this != &rhs)
    {
	this->destroy();
	this->init(rhs.size);
	if (this->size)
	{
	    memcpy(this->buf, rhs.buf, this->size);
	}
    }
}

void
Buffer::destroy()
{
    delete [] this->buf;
    this->size = 0;
    this->buf = 0;
}

DLL_EXPORT
unsigned long
Buffer::getSize() const
{
    return this->size;
}

DLL_EXPORT
unsigned char const*
Buffer::getBuffer() const
{
    return this->buf;
}

DLL_EXPORT
unsigned char*
Buffer::getBuffer()
{
    return this->buf;
}

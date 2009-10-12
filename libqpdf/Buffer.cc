#include <qpdf/Buffer.hh>

#include <string.h>

Buffer::Buffer()
{
    init(0);
}

Buffer::Buffer(unsigned long size)
{
    init(size);
}

Buffer::Buffer(Buffer const& rhs)
{
    init(0);
    copy(rhs);
}

Buffer&
Buffer::operator=(Buffer const& rhs)
{
    copy(rhs);
    return *this;
}

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

unsigned long
Buffer::getSize() const
{
    return this->size;
}

unsigned char const*
Buffer::getBuffer() const
{
    return this->buf;
}

unsigned char*
Buffer::getBuffer()
{
    return this->buf;
}

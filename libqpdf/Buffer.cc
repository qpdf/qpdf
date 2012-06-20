#include <qpdf/Buffer.hh>

#include <string.h>

Buffer::Buffer()
{
    init(0, 0, true);
}

Buffer::Buffer(size_t size)
{
    init(size, 0, true);
}

Buffer::Buffer(unsigned char* buf, size_t size)
{
    init(size, buf, false);
}

Buffer::Buffer(Buffer const& rhs)
{
    init(0, 0, true);
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
Buffer::init(size_t size, unsigned char* buf, bool own_memory)
{
    this->own_memory = own_memory;
    this->size = size;
    if (own_memory)
    {
	this->buf = (size ? new unsigned char[size] : 0);
    }
    else
    {
	this->buf = buf;
    }
}

void
Buffer::copy(Buffer const& rhs)
{
    if (this != &rhs)
    {
	this->destroy();
	this->init(rhs.size, 0, true);
	if (this->size)
	{
	    memcpy(this->buf, rhs.buf, this->size);
	}
    }
}

void
Buffer::destroy()
{
    if (this->own_memory)
    {
	delete [] this->buf;
    }
    this->size = 0;
    this->buf = 0;
}

size_t
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

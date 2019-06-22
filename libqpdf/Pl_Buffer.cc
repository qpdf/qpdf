#include <qpdf/Pl_Buffer.hh>
#include <stdexcept>
#include <algorithm>
#include <assert.h>
#include <string.h>

Pl_Buffer::Members::Members() :
    ready(true),
    total_size(0)
{
}

Pl_Buffer::Members::~Members()
{
}

Pl_Buffer::Pl_Buffer(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    m(new Members())
{
}

Pl_Buffer::~Pl_Buffer()
{
}

void
Pl_Buffer::write(unsigned char* buf, size_t len)
{
    if (this->m->data.getPointer() == 0)
    {
        this->m->data = new Buffer(len);
    }
    size_t cur_size = this->m->data->getSize();
    size_t left = cur_size - this->m->total_size;
    if (left < len)
    {
        size_t new_size = std::max(this->m->total_size + len, 2 * cur_size);
        PointerHolder<Buffer> b = new Buffer(new_size);
        memcpy(b->getBuffer(), this->m->data->getBuffer(), this->m->total_size);
        this->m->data = b;
    }
    if (len)
    {
        memcpy(this->m->data->getBuffer() + this->m->total_size, buf, len);
        this->m->total_size += len;
    }
    this->m->ready = false;

    if (getNext(true))
    {
	getNext()->write(buf, len);
    }
}

void
Pl_Buffer::finish()
{
    this->m->ready = true;
    if (getNext(true))
    {
	getNext()->finish();
    }
}

Buffer*
Pl_Buffer::getBuffer()
{
    if (! this->m->ready)
    {
	throw std::logic_error("Pl_Buffer::getBuffer() called when not ready");
    }

    Buffer* b = new Buffer(this->m->total_size);
    if (this->m->total_size > 0)
    {
        unsigned char* p = b->getBuffer();
        memcpy(p, this->m->data->getBuffer(), this->m->total_size);
    }
    this->m = new Members();
    return b;
}

#include <qpdf/Pl_Buffer.hh>
#include <stdexcept>
#include <algorithm>
#include <assert.h>
#include <string.h>

Pl_Buffer::Pl_Buffer(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    ready(true),
    total_size(0)
{
}

Pl_Buffer::~Pl_Buffer()
{
}

void
Pl_Buffer::write(unsigned char* buf, size_t len)
{
    PointerHolder<Buffer> cur_buf;
    size_t cur_size = 0;
    if (! this->data.empty())
    {
        cur_buf = this->data.back();
        cur_size = cur_buf->getSize();
    }
    size_t left = cur_size - this->total_size;
    if (left < len)
    {
        size_t new_size = std::max(this->total_size + len, 2 * cur_size);
        Buffer* b = new Buffer(new_size);
        if (cur_buf.getPointer())
        {
            memcpy(b->getBuffer(), cur_buf->getBuffer(), this->total_size);
        }
        this->data.clear();
        cur_buf = b;
        this->data.push_back(cur_buf);
    }
    if (len)
    {
        memcpy(cur_buf->getBuffer() + this->total_size, buf, len);
        this->total_size += len;
    }
    this->ready = false;

    if (getNext(true))
    {
	getNext()->write(buf, len);
    }
}

void
Pl_Buffer::finish()
{
    this->ready = true;
    if (getNext(true))
    {
	getNext()->finish();
    }
}

Buffer*
Pl_Buffer::getBuffer()
{
    if (! this->ready)
    {
	throw std::logic_error("Pl_Buffer::getBuffer() called when not ready");
    }

    Buffer* b = new Buffer(this->total_size);
    unsigned char* p = b->getBuffer();
    if (! this->data.empty())
    {
        PointerHolder<Buffer> bp = this->data.back();
        this->data.clear();
        memcpy(p, bp->getBuffer(), this->total_size);
    }
    this->total_size = 0;
    this->ready = false;

    return b;
}

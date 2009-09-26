
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QEXC.hh>
#include <assert.h>
#include <string.h>

DLL_EXPORT
Pl_Buffer::Pl_Buffer(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    ready(false),
    total_size(0)
{
}

DLL_EXPORT
Pl_Buffer::~Pl_Buffer()
{
}

DLL_EXPORT
void
Pl_Buffer::write(unsigned char* buf, int len)
{
    Buffer* b = new Buffer(len);
    memcpy(b->getBuffer(), buf, len);
    this->data.push_back(b);
    this->ready = false;
    this->total_size += len;

    if (getNext(true))
    {
	getNext()->write(buf, len);
    }
}

DLL_EXPORT
void
Pl_Buffer::finish()
{
    this->ready = true;
    if (getNext(true))
    {
	getNext()->finish();
    }
}

DLL_EXPORT
Buffer*
Pl_Buffer::getBuffer()
{
    if (! this->ready)
    {
	throw QEXC::Internal("Pl_Buffer::getBuffer() called when not ready");
    }

    Buffer* b = new Buffer(this->total_size);
    unsigned char* p = b->getBuffer();
    while (! this->data.empty())
    {
	PointerHolder<Buffer> bph = this->data.front();
	this->data.pop_front();
	Buffer* bp = bph.getPointer();
	size_t bytes = bp->getSize();
	memcpy(p, bp->getBuffer(), bytes);
	p += bytes;
	this->total_size -= bytes;
    }

    assert(this->total_size == 0);
    this->ready = false;

    return b;
}

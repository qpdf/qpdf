#include <qpdf/Pl_Buffer.hh>

#include <algorithm>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>

Pl_Buffer::Pl_Buffer(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    m(new Members())
{
}

Pl_Buffer::~Pl_Buffer()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_Buffer::write(unsigned char const* buf, size_t len)
{
    this->m->data.append(buf, len);
    this->m->ready = false;

    if (getNext(true)) {
        getNext()->write(buf, len);
    }
}

void
Pl_Buffer::finish()
{
    this->m->ready = true;
    if (getNext(true)) {
        getNext()->finish();
    }
}

Buffer*
Pl_Buffer::getBuffer()
{
    if (!this->m->ready) {
        throw std::logic_error("Pl_Buffer::getBuffer() called when not ready");
    }

    auto size = this->m->data.length();
    Buffer* b = new Buffer(size);
    if (size > 0) {
        unsigned char* p = b->getBuffer();
        memcpy(p, this->m->data.data(), size);
    }
    this->m->data.clear();
    return b;
}

std::shared_ptr<Buffer>
Pl_Buffer::getBufferSharedPointer()
{
    return std::shared_ptr<Buffer>(getBuffer());
}

void
Pl_Buffer::getMallocBuffer(unsigned char** buf, size_t* len)
{
    if (!this->m->ready) {
        throw std::logic_error(
            "Pl_Buffer::getMallocBuffer() called when not ready");
    }
    auto size = this->m->data.length();
    *len = size;
    if (size > 0) {
        *buf = reinterpret_cast<unsigned char*>(malloc(size));
        memcpy(*buf, this->m->data.data(), size);
    } else {
        *buf = nullptr;
    }
    this->m->data.clear();
}

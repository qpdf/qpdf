#include <qpdf/Buffer.hh>

#include <cstring>

Buffer::Members::Members(size_t size, unsigned char* buf, bool own_memory) :
    own_memory(own_memory),
    size(size),
    buf(nullptr)
{
    if (own_memory) {
        this->buf = (size != 0u ? new unsigned char[size] : nullptr);
    } else {
        this->buf = buf;
    }
}

Buffer::Members::~Members()
{
    if (this->own_memory) {
        delete[] this->buf;
    }
}

Buffer::Buffer() :
    m(new Members(0, nullptr, true))
{
}

Buffer::Buffer(size_t size) :
    m(new Members(size, nullptr, true))
{
}

Buffer::Buffer(unsigned char* buf, size_t size) :
    m(new Members(size, buf, false))
{
}

Buffer::Buffer(Buffer const& rhs)
{
    copy(rhs);
}

Buffer&
Buffer::operator=(Buffer const& rhs)
{
    copy(rhs);
    return *this;
}

void
Buffer::copy(Buffer const& rhs)
{
    if (this != &rhs) {
        this->m =
            std::shared_ptr<Members>(new Members(rhs.m->size, nullptr, true));
        if (this->m->size != 0u) {
            memcpy(this->m->buf, rhs.m->buf, this->m->size);
        }
    }
}

size_t
Buffer::getSize() const
{
    return this->m->size;
}

unsigned char const*
Buffer::getBuffer() const
{
    return this->m->buf;
}

unsigned char*
Buffer::getBuffer()
{
    return this->m->buf;
}

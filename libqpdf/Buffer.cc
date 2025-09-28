#include <qpdf/assert_test.h>

#include <qpdf/Buffer.hh>

#include <cstring>

class Buffer::Members
{
    friend class Buffer;

  public:
    Members() = default;
    // Constructor for Buffers that don't own the memory.
    Members(size_t size, unsigned char* buf) :
        size(size),
        buf(buf)
    {
    }
    Members(std::string&& content) :
        str(std::move(content)),
        size(str.size()),
        buf(reinterpret_cast<unsigned char*>(str.data()))
    {
    }
    Members(Members const&) = delete;
    ~Members() = default;

  private:
    std::string str;
    size_t size;
    unsigned char* buf;
};

Buffer::Buffer() :
    m(std::make_unique<Members>())
{
}

Buffer::Buffer(size_t size) :
    m(std::make_unique<Members>(std::string(size, '\0')))
{
}

Buffer::Buffer(std::string&& content) :
    m(std::make_unique<Members>(std::move(content)))
{
}

Buffer::Buffer(unsigned char* buf, size_t size) :
    m(std::make_unique<Members>(size, buf))
{
}

Buffer::Buffer(std::string& content) :
    m(std::make_unique<Members>(content.size(), reinterpret_cast<unsigned char*>(content.data())))
{
}

Buffer::Buffer(Buffer&& rhs) noexcept :
    m(std::move(rhs.m))
{
}

Buffer&
Buffer::operator=(Buffer&& rhs) noexcept
{
    std::swap(m, rhs.m);
    return *this;
}

Buffer::~Buffer() = default;

size_t
Buffer::getSize() const
{
    return m->size;
}

unsigned char const*
Buffer::getBuffer() const
{
    return m->buf;
}

unsigned char*
Buffer::getBuffer()
{
    return m->buf;
}

Buffer
Buffer::copy() const
{
    if (m->size == 0) {
        return {};
    }
    return {std::string(reinterpret_cast<char const*>(m->buf), m->size)};
}

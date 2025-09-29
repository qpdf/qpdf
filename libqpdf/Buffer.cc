#include <qpdf/assert_test.h>

#include <qpdf/Buffer.hh>

class Buffer::Members
{
    friend class Buffer;

  public:
    Members() = default;
    // Constructor for Buffers that don't own the memory.
    Members(size_t size, char* buf) :
        size(size),
        buf(buf)
    {
    }
    Members(std::string&& content) :
        str(std::move(content)),
        size(str.size()),
        buf(str.data())
    {
    }
    Members(Members const&) = delete;
    ~Members() = default;

  private:
    std::string str;
    size_t size;
    char* buf;
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
    m(std::make_unique<Members>(size, reinterpret_cast<char*>(buf)))
{
}

Buffer::Buffer(std::string& content) :
    m(std::make_unique<Members>(content.size(), content.data()))
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
    return reinterpret_cast<unsigned char*>(m->buf);
}

unsigned char*
Buffer::getBuffer()
{
    return reinterpret_cast<unsigned char*>(m->buf);
}

Buffer
Buffer::copy() const
{
    if (m->size == 0) {
        return {};
    }
    return {std::string(m->buf, m->size)};
}

std::string
Buffer::move()
{
    if (m->size == 0) {
        return {};
    }
    if (!m->str.empty()) {
        m->size = 0;
        m->buf = nullptr;
        return std::move(m->str);
    }
    return {m->buf, m->size};
}

std::string_view
Buffer::view() const
{
    if (!m->buf) {
        return {};
    }
    return {m->buf, m->size};
}

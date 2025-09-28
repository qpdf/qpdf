#include <qpdf/assert_test.h>

#include <qpdf/Buffer.hh>

#include <cstring>

class Buffer::Members
{
    friend class Buffer;

  public:
    Members(size_t size, unsigned char* buf, bool own_memory);
    Members(std::string&& content);
    Members(Members const&) = delete;
    ~Members();

  private:

    std::string str;
    bool own_memory;
    size_t size;
    unsigned char* buf;
};

Buffer::Members::Members(size_t size, unsigned char* buf, bool own_memory) :
    own_memory(own_memory),
    size(size),
    buf(nullptr)
{
    if (own_memory) {
        this->buf = (size ? new unsigned char[size] : nullptr);
    } else {
        this->buf = buf;
    }
}

Buffer::Members::Members(std::string&& content) :
    str(std::move(content)),
    own_memory(false),
    size(str.size()),
    buf(reinterpret_cast<unsigned char*>(str.data()))
{
}

Buffer::Members::~Members()
{
    if (this->own_memory) {
        delete[] this->buf;
    }
}

Buffer::Buffer() :
    m(std::make_unique<Members>(0, nullptr, true))
{
}

Buffer::Buffer(size_t size) :
    m(std::make_unique<Members>(size, nullptr, true))
{
}

Buffer::Buffer(std::string&& content) :
    m(std::make_unique<Members>(std::move(content)))
{
}

Buffer::Buffer(unsigned char* buf, size_t size) :
    m(std::make_unique<Members>(size, buf, false))
{
}

Buffer::Buffer(std::string& content) :
    m(std::make_unique<Members>(content.size(), reinterpret_cast<unsigned char*>(content.data()), false))
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

void
Buffer::copy(Buffer const& rhs)
{
    if (this != &rhs) {
        m = std::make_unique<Members>(rhs.m->size, nullptr, true);
        if (m->size) {
            memcpy(m->buf, rhs.m->buf, m->size);
        }
    }
}

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
    auto result = Buffer(m->size);
    if (m->size) {
        memcpy(result.m->buf, m->buf, m->size);
    }
    return result;
}

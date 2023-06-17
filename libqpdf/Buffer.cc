#include <qpdf/assert_test.h>

#include <qpdf/Buffer.hh>

#include <cstring>

bool test_mode = false;

// During CI the Buffer copy constructor and copy assignment operator throw an assertion error to
// detect their accidental use. Call setTestMode to surpress the assertion errors for testing of
// copy construction and assignment.
void
Buffer::setTestMode() noexcept
{
    test_mode = true;
}

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
    assert(test_mode);
    copy(rhs);
}

Buffer&
Buffer::operator=(Buffer const& rhs)
{
    assert(test_mode);
    copy(rhs);
    return *this;
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

void
Buffer::copy(Buffer const& rhs)
{
    if (this != &rhs) {
        m = std::unique_ptr<Members>(new Members(rhs.m->size, nullptr, true));
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

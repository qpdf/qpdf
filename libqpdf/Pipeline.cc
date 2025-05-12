#include <qpdf/Pipeline.hh>

#include <cstring>
#include <stdexcept>

Pipeline::Pipeline(char const* identifier, Pipeline* next) :
    identifier(identifier),
    next_(next)
{
}

Pipeline*
Pipeline::getNext(bool allow_null)
{
    if (!next_ && !allow_null) {
        throw std::logic_error(
            identifier + ": Pipeline::getNext() called on pipeline with no next");
    }
    return next_;
}

std::string
Pipeline::getIdentifier() const
{
    return identifier;
}

void
Pipeline::writeCStr(char const* cstr)
{
    write(cstr, strlen(cstr));
}

void
Pipeline::writeString(std::string const& str)
{
    write(str.c_str(), str.length());
}

Pipeline&
Pipeline::operator<<(char const* cstr)
{
    writeCStr(cstr);
    return *this;
}

Pipeline&
Pipeline::operator<<(std::string const& str)
{
    writeString(str);
    return *this;
}

Pipeline&
Pipeline::operator<<(short i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(int i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(long i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(long long i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned short i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned int i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned long i)
{
    writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned long long i)
{
    writeString(std::to_string(i));
    return *this;
}

void
Pipeline::write(char const* data, size_t len)
{
    write(reinterpret_cast<unsigned char const*>(data), len);
}

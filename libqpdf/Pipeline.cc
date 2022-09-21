#include <qpdf/Pipeline.hh>

#include <qpdf/QUtil.hh>

#include <cstring>
#include <stdexcept>

Pipeline::Pipeline(char const* identifier, Pipeline* next) :
    identifier(identifier),
    next(next)
{
}

Pipeline*
Pipeline::getNext(bool allow_null)
{
    if ((this->next == nullptr) && (!allow_null)) {
        throw std::logic_error(
            this->identifier +
            ": Pipeline::getNext() called on pipeline with no next");
    }
    return this->next;
}

std::string
Pipeline::getIdentifier() const
{
    return this->identifier;
}

void
Pipeline::writeCStr(char const* cstr)
{
    this->write(cstr, strlen(cstr));
}

void
Pipeline::writeString(std::string const& str)
{
    this->write(str.c_str(), str.length());
}

Pipeline&
Pipeline::operator<<(char const* cstr)
{
    this->writeCStr(cstr);
    return *this;
}

Pipeline&
Pipeline::operator<<(std::string const& str)
{
    this->writeString(str);
    return *this;
}

Pipeline&
Pipeline::operator<<(short i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(int i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(long i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(long long i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned short i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned int i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned long i)
{
    this->writeString(std::to_string(i));
    return *this;
}

Pipeline&
Pipeline::operator<<(unsigned long long i)
{
    this->writeString(std::to_string(i));
    return *this;
}

void
Pipeline::write(char const* data, size_t len)
{
    this->write(reinterpret_cast<unsigned char const*>(data), len);
}

#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QUtil.hh>
#include <string.h>

QPDFSystemError::QPDFSystemError(std::string const& description,
                                 int system_errno) :
    std::runtime_error(createWhat(description, system_errno)),
    description(description),
    system_errno(system_errno)
{
}

QPDFSystemError::~QPDFSystemError() throw ()
{
}

std::string
QPDFSystemError::createWhat(std::string const& description,
                            int system_errno)
{
    std::string message;
#ifdef _MSC_VER
    // "94" is mentioned in the MSVC docs, but it's still safe if the
    // message is longer.  strerror_s is a templated function that
    // knows the size of buf and truncates.
    char buf[94];
    if (strerror_s(buf, system_errno) != 0)
    {
        message = description + ": failed with an unknown error";
    }
    else
    {
        message = description + ": " + buf;
    }
#else
    message = description + ": " + strerror(system_errno);
#endif
    return message;
}

std::string const&
QPDFSystemError::getDescription() const
{
    return this->description;
}

int
QPDFSystemError::getErrno() const
{
    return this->system_errno;
}

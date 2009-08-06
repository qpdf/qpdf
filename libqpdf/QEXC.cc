
#include <qpdf/QEXC.hh>
#include <string.h>
#include <errno.h>

DLL_EXPORT
QEXC::Base::Base()
{
    // nothing needed
}

DLL_EXPORT
QEXC::Base::Base(std::string const& message) :
    message(message)
{
    // nothing needed
}

DLL_EXPORT
std::string const&
QEXC::Base::unparse() const
{
    return this->message;
}

DLL_EXPORT
void
QEXC::Base::setMessage(std::string const& message)
{
    this->message = message;
}

DLL_EXPORT
const char*
QEXC::Base::what() const throw()
{
    // Since unparse() returns a const string reference, its
    // implementors must arrange to have it return a reference to a
    // string that is not going to disappear.  It is therefore safe
    // for us to return it's c_str() pointer.
    return this->unparse().c_str();
}

DLL_EXPORT
QEXC::General::General()
{
    // nothing needed
}

DLL_EXPORT
QEXC::General::General(std::string const& message) :
    Base(message)
{
    // nothing needed
}

DLL_EXPORT
QEXC::System::System(std::string const& prefix, int sys_errno)
{
    // Note: using sys_errno in case errno is a macro.
    this->sys_errno = sys_errno;
    this->setMessage(prefix + ": " + strerror(sys_errno));
}

DLL_EXPORT
int
QEXC::System::getErrno() const
{
    return this->sys_errno;
}

DLL_EXPORT
QEXC::Internal::Internal(std::string const& message) :
    Base("INTERNAL ERROR: " + message)
{
    // nothing needed
}

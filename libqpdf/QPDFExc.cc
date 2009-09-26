#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

DLL_EXPORT
QPDFExc::QPDFExc(std::string const& message) :
    std::runtime_error(message)
{
}

DLL_EXPORT
QPDFExc::QPDFExc(std::string const& filename, int offset,
		 std::string const& message) :
    std::runtime_error(filename + ": offset " + QUtil::int_to_string(offset) +
		       ": " + message)
{
}

DLL_EXPORT
QPDFExc::~QPDFExc() throw ()
{
}

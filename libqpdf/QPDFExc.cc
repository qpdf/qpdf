#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

QPDFExc::QPDFExc(std::string const& message) :
    std::runtime_error(message)
{
}

QPDFExc::QPDFExc(std::string const& filename, int offset,
		 std::string const& message) :
    std::runtime_error(filename + ": offset " + QUtil::int_to_string(offset) +
		       ": " + message)
{
}

QPDFExc::~QPDFExc() throw ()
{
}

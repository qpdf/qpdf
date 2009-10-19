#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

QPDFExc::QPDFExc(qpdf_error_code_e error_code,
		 std::string const& filename,
		 std::string const& object,
		 off_t offset,
		 std::string const& message) :
    std::runtime_error(createWhat(filename, object, offset, message)),
    error_code(error_code),
    filename(filename),
    object(object),
    offset(offset),
    message(message)
{
}

QPDFExc::~QPDFExc() throw ()
{
}

std::string
QPDFExc::createWhat(std::string const& filename,
		    std::string const& object,
		    off_t offset,
		    std::string const& message)
{
    std::string result;
    if (! filename.empty())
    {
	result += filename;
    }
    if (! (object.empty() && offset == 0))
    {
	result += " (";
	if (! object.empty())
	{
	    result += object;
	    if (offset > 0)
	    {
		result += ", ";
	    }
	}
	if (offset > 0)
	{
	    result += "file position " + QUtil::int_to_string(offset);
	}
	result += ")";
    }
    if (! result.empty())
    {
	result += ": ";
    }
    result += message;
    return result;
}

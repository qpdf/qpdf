#include <qpdf/QPDF_Reserved.hh>
#include <stdexcept>

QPDF_Reserved::~QPDF_Reserved()
{
}

std::string
QPDF_Reserved::unparse()
{
    throw std::logic_error("attempt to unparse QPDF_Reserved");
    return "";
}

QPDFObject::object_type_e
QPDF_Reserved::getTypeCode() const
{
    return QPDFObject::ot_reserved;
}

char const*
QPDF_Reserved::getTypeName() const
{
    return "reserved";
}

#include <qpdf/QPDF_Null.hh>

QPDF_Null::~QPDF_Null()
{
}

std::string
QPDF_Null::unparse()
{
    return "null";
}

QPDFObject::object_type_e
QPDF_Null::getTypeCode() const
{
    return QPDFObject::ot_null;
}

char const*
QPDF_Null::getTypeName() const
{
    return "null";
}

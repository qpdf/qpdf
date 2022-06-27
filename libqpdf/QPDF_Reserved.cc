#include <qpdf/QPDF_Reserved.hh>

#include <stdexcept>

std::shared_ptr<QPDFObject>
QPDF_Reserved::create()
{
    return do_create(new QPDF_Reserved());
}

std::shared_ptr<QPDFObject>
QPDF_Reserved::shallowCopy()
{
    return create();
}

std::string
QPDF_Reserved::unparse()
{
    throw std::logic_error("attempt to unparse QPDF_Reserved");
    return "";
}

JSON
QPDF_Reserved::getJSON(int json_version)
{
    throw std::logic_error("attempt to generate JSON from QPDF_Reserved");
    return JSON::makeNull();
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

#include <qpdf/QPDF_Reserved.hh>

#include <stdexcept>

QPDF_Reserved::QPDF_Reserved() :
    QPDFValue(::ot_reserved, "reserved")
{
}

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
    throw std::logic_error(
        "QPDFObjectHandle: attempting to unparse a reserved object");
    return "";
}

JSON
QPDF_Reserved::getJSON(int json_version)
{
    throw std::logic_error(
        "QPDFObjectHandle: attempting to unparse a reserved object");
    return JSON::makeNull();
}

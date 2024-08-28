#include <qpdf/QPDF_Reserved.hh>

#include <stdexcept>

QPDF_Reserved::QPDF_Reserved() :
    QPDFValue(::ot_reserved)
{
}

std::shared_ptr<QPDFObject>
QPDF_Reserved::create()
{
    return do_create(new QPDF_Reserved());
}

std::shared_ptr<QPDFObject>
QPDF_Reserved::copy(bool shallow)
{
    return create();
}

std::string
QPDF_Reserved::unparse()
{
    throw std::logic_error("QPDFObjectHandle: attempting to unparse a reserved object");
    return "";
}

void
QPDF_Reserved::writeJSON(int json_version, JSON::Writer& p)
{
    throw std::logic_error("QPDFObjectHandle: attempting to get JSON from a reserved object");
}

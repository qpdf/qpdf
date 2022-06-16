#include <qpdf/QPDF_Null.hh>

std::shared_ptr<QPDFObject>
QPDF_Null::create()
{
    return do_create(new QPDF_Null());
}

std::shared_ptr<QPDFObject>
QPDF_Null::shallowCopy()
{
    return create();
}

std::string
QPDF_Null::unparse()
{
    return "null";
}

JSON
QPDF_Null::getJSON(int json_version)
{
    return JSON::makeNull();
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

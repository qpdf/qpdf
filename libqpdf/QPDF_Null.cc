#include <qpdf/QPDF_Null.hh>

QPDF_Null::QPDF_Null() :
    QPDFValue(::ot_null, "null")
{
}

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

#include <qpdf/QPDF_Bool.hh>

QPDF_Bool::QPDF_Bool(bool val) :
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Bool::create(bool value)
{
    return do_create(new QPDF_Bool(value));
}

std::shared_ptr<QPDFObject>
QPDF_Bool::shallowCopy()
{
    return create(val);
}

std::string
QPDF_Bool::unparse()
{
    return (val ? "true" : "false");
}

JSON
QPDF_Bool::getJSON(int json_version)
{
    return JSON::makeBool(this->val);
}

QPDFObject::object_type_e
QPDF_Bool::getTypeCode() const
{
    return QPDFObject::ot_boolean;
}

char const*
QPDF_Bool::getTypeName() const
{
    return "boolean";
}

bool
QPDF_Bool::getVal() const
{
    return this->val;
}

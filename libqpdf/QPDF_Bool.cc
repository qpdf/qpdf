#include <qpdf/QPDF_Bool.hh>

QPDF_Bool::QPDF_Bool(bool val) :
    QPDFValue(::ot_boolean, "boolean"),
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Bool::create(bool value)
{
    return do_create(new QPDF_Bool(value));
}

std::shared_ptr<QPDFObject>
QPDF_Bool::copy(bool shallow)
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

bool
QPDF_Bool::getVal() const
{
    return this->val;
}

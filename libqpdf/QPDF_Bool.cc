#include <qpdf/QPDF_Bool.hh>

#include <qpdf/JSON_writer.hh>

QPDF_Bool::QPDF_Bool(bool val) :
    QPDFValue(::ot_boolean),
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

void
QPDF_Bool::writeJSON(int json_version, JSON::Writer& p)
{
    p << val;
}

bool
QPDF_Bool::getVal() const
{
    return this->val;
}

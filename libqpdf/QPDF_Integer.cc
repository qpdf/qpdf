#include <qpdf/QPDF_Integer.hh>

#include <qpdf/QUtil.hh>

QPDF_Integer::QPDF_Integer(long long val) :
    QPDFValue(::ot_integer, "integer"),
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Integer::create(long long value)
{
    return do_create(new QPDF_Integer(value));
}

std::shared_ptr<QPDFObject>
QPDF_Integer::copy(bool shallow)
{
    return create(val);
}

std::string
QPDF_Integer::unparse()
{
    return std::to_string(this->val);
}

JSON
QPDF_Integer::getJSON(int json_version)
{
    return JSON::makeInt(this->val);
}

long long
QPDF_Integer::getVal() const
{
    return this->val;
}

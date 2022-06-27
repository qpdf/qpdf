#include <qpdf/QPDF_Integer.hh>

#include <qpdf/QUtil.hh>

QPDF_Integer::QPDF_Integer(long long val) :
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Integer::create(long long value)
{
    return do_create(new QPDF_Integer(value));
}

std::shared_ptr<QPDFObject>
QPDF_Integer::shallowCopy()
{
    return create(val);
}

std::string
QPDF_Integer::unparse()
{
    return QUtil::int_to_string(this->val);
}

JSON
QPDF_Integer::getJSON(int json_version)
{
    return JSON::makeInt(this->val);
}

QPDFObject::object_type_e
QPDF_Integer::getTypeCode() const
{
    return QPDFObject::ot_integer;
}

char const*
QPDF_Integer::getTypeName() const
{
    return "integer";
}

long long
QPDF_Integer::getVal() const
{
    return this->val;
}

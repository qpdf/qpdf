#include <qpdf/QPDF_Integer.hh>

#include <qpdf/QUtil.hh>

QPDF_Integer::QPDF_Integer(long long val) :
    val(val)
{
}

QPDF_Integer::~QPDF_Integer()
{
}

std::string
QPDF_Integer::unparse()
{
    return QUtil::int_to_string(this->val);
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

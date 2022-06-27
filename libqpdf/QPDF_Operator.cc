#include <qpdf/QPDF_Operator.hh>

QPDF_Operator::QPDF_Operator(std::string const& val) :
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Operator::create(std::string const& val)
{
    return do_create(new QPDF_Operator(val));
}

std::shared_ptr<QPDFObject>
QPDF_Operator::shallowCopy()
{
    return create(val);
}

std::string
QPDF_Operator::unparse()
{
    return this->val;
}

JSON
QPDF_Operator::getJSON(int json_version)
{
    return JSON::makeNull();
}

QPDFObject::object_type_e
QPDF_Operator::getTypeCode() const
{
    return QPDFObject::ot_operator;
}

char const*
QPDF_Operator::getTypeName() const
{
    return "operator";
}

std::string
QPDF_Operator::getVal() const
{
    return this->val;
}

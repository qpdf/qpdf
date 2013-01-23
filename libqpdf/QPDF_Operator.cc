#include <qpdf/QPDF_Operator.hh>

#include <qpdf/QUtil.hh>

QPDF_Operator::QPDF_Operator(std::string const& val) :
    val(val)
{
}

QPDF_Operator::~QPDF_Operator()
{
}

std::string
QPDF_Operator::unparse()
{
    return this->val;
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

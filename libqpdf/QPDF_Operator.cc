#include <qpdf/QPDF_Operator.hh>

QPDF_Operator::QPDF_Operator(std::string const& val) :
    QPDFValue(::ot_operator, "operator"),
    val(val)
{
}

std::shared_ptr<QPDFValueProxy>
QPDF_Operator::create(std::string const& val)
{
    return do_create(new QPDF_Operator(val));
}

std::shared_ptr<QPDFValueProxy>
QPDF_Operator::shallowCopy()
{
    return create(val);
}

std::string
QPDF_Operator::unparse()
{
    return val;
}

JSON
QPDF_Operator::getJSON(int json_version)
{
    return JSON::makeNull();
}

std::string
QPDF_Operator::getVal() const
{
    return this->val;
}

#include <qpdf/QPDF_Operator.hh>

QPDF_Operator::QPDF_Operator(std::string_view val) :
    QPDFValue(::ot_operator, "operator"),
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Operator::create(std::string_view val)
{
    return do_create(new QPDF_Operator(val));
}

std::shared_ptr<QPDFObject>
QPDF_Operator::copy(bool shallow)
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

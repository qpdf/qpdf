#include <qpdf/QPDF_Operator.hh>

#include <qpdf/JSON_writer.hh>

QPDF_Operator::QPDF_Operator(std::string const& val) :
    QPDFValue(::ot_operator),
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_Operator::create(std::string const& val)
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

void
QPDF_Operator::writeJSON(int json_version, JSON::Writer& p)
{
    p << "null";
}

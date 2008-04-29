
#include <qpdf/QPDF_Real.hh>

QPDF_Real::QPDF_Real(std::string const& val) :
    val(val)
{
}

QPDF_Real::~QPDF_Real()
{
}

std::string
QPDF_Real::unparse()
{
    return this->val;
}

std::string
QPDF_Real::getVal()
{
    return this->val;
}

#include <qpdf/QPDF_Bool.hh>

QPDF_Bool::QPDF_Bool(bool val) :
    val(val)
{
}

QPDF_Bool::~QPDF_Bool()
{
}

std::string
QPDF_Bool::unparse()
{
    return (val ? "true" : "false");
}

bool
QPDF_Bool::getVal() const
{
    return this->val;
}

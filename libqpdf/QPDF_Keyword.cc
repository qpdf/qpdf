#include <qpdf/QPDF_Keyword.hh>

#include <qpdf/QUtil.hh>

QPDF_Keyword::QPDF_Keyword(std::string const& val) :
    val(val)
{
}

QPDF_Keyword::~QPDF_Keyword()
{
}

std::string
QPDF_Keyword::unparse()
{
    return this->val;
}

std::string
QPDF_Keyword::getVal() const
{
    return this->val;
}

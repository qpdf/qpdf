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

QPDFObject::object_type_e
QPDF_Keyword::getTypeCode() const
{
    return QPDFObject::ot_keyword;
}

char const*
QPDF_Keyword::getTypeName() const
{
    return "keyword";
}

std::string
QPDF_Keyword::getVal() const
{
    return this->val;
}

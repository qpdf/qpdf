#include <qpdf/QPDF_InlineImage.hh>

#include <qpdf/QUtil.hh>

QPDF_InlineImage::QPDF_InlineImage(std::string const& val) :
    val(val)
{
}

QPDF_InlineImage::~QPDF_InlineImage()
{
}

std::string
QPDF_InlineImage::unparse()
{
    return this->val;
}

std::string
QPDF_InlineImage::getVal() const
{
    return this->val;
}

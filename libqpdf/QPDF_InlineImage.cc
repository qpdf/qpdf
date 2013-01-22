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

QPDFObject::object_type_e
QPDF_InlineImage::getTypeCode() const
{
    return QPDFObject::ot_inlineimage;
}

char const*
QPDF_InlineImage::getTypeName() const
{
    return "inline-image";
}

std::string
QPDF_InlineImage::getVal() const
{
    return this->val;
}

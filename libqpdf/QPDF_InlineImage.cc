#include <qpdf/QPDF_InlineImage.hh>

QPDF_InlineImage::QPDF_InlineImage(std::string const& val) :
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_InlineImage::create(std::string const& val)
{
    return do_create(new QPDF_InlineImage(val));
}

std::shared_ptr<QPDFObject>
QPDF_InlineImage::shallowCopy()
{
    return create(val);
}

std::string
QPDF_InlineImage::unparse()
{
    return this->val;
}

JSON
QPDF_InlineImage::getJSON(int json_version)
{
    return JSON::makeNull();
}

qpdf_object_type_e
QPDF_InlineImage::getTypeCode() const
{
    return ::ot_inlineimage;
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

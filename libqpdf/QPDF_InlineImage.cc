#include <qpdf/QPDF_InlineImage.hh>

#include <qpdf/JSON_writer.hh>

QPDF_InlineImage::QPDF_InlineImage(std::string const& val) :
    QPDFValue(::ot_inlineimage),
    val(val)
{
}

std::shared_ptr<QPDFObject>
QPDF_InlineImage::create(std::string const& val)
{
    return do_create(new QPDF_InlineImage(val));
}

std::shared_ptr<QPDFObject>
QPDF_InlineImage::copy(bool shallow)
{
    return create(val);
}

std::string
QPDF_InlineImage::unparse()
{
    return this->val;
}

void
QPDF_InlineImage::writeJSON(int json_version, JSON::Writer& p)
{
    p << "null";
}

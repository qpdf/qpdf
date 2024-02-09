#include <qpdf/QPDF_InlineImage.hh>

#include <qpdf/JSON_writer.hh>

QPDF_InlineImage::QPDF_InlineImage(std::string const& val) :
    QPDFValue(::ot_inlineimage, "inline-image"),
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

JSON
QPDF_InlineImage::getJSON(int json_version)
{
    return JSON::makeNull();
}

void
QPDF_InlineImage::writeJSON(int json_version, JSON::Writer& p)
{
    p << "null";
}

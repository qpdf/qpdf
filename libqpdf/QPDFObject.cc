#include <qpdf/QPDFObject.hh>

QPDFObject::QPDFObject() :
///    owning_qpdf(0),
    parsed_offset(-1)
{
}

void
QPDFObject::setDescription(QPDF* qpdf, std::string const& description)
{
///    this->owning_qpdf = qpdf;
///    this->object_description = description;
}

bool
QPDFObject::getDescription(QPDF*& qpdf, std::string& description)
{
///    qpdf = this->owning_qpdf;
///    description = this->object_description;
///    return this->owning_qpdf != 0;
    return false;
}

bool
QPDFObject::hasDescription()
{
///    return this->owning_qpdf != 0;
    return false;
}

void
QPDFObject::setParsedOffset(qpdf_offset_t offset)
{
    this->parsed_offset = offset;
}

qpdf_offset_t
QPDFObject::getParsedOffset()
{
    return this->parsed_offset;
}

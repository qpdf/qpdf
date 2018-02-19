#include <qpdf/QPDFObject.hh>

QPDFObject::Members::Members() :
    owning_qpdf(0)
{
}

QPDFObject::Members::~Members()
{
}

QPDFObject::QPDFObject() :
    m(new Members)
{
}

void
QPDFObject::setDescription(QPDF* qpdf, std::string const& description)
{
    this->m->owning_qpdf = qpdf;
    this->m->object_description = description;
}

bool
QPDFObject::getDescription(QPDF*& qpdf, std::string& description)
{
    qpdf = this->m->owning_qpdf;
    description = this->m->object_description;
    return this->m->owning_qpdf;
}

bool
QPDFObject::hasDescription()
{
    return this->m->owning_qpdf;
}

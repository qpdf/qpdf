#include <qpdf/QPDFObject_private.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDF_Destroyed.hh>

void
QPDFObject::doResolve()
{
    auto og = value->og;
    QPDF::Resolver::resolve(value->qpdf, og);
}

void
QPDFObject::destroy()
{
    value = QPDF_Destroyed::getInstance();
}

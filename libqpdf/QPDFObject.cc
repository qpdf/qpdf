#include <qpdf/QPDFObject_private.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDF_Destroyed.hh>

void
QPDFObject::doResolve()
{
    QPDF::Resolver::resolve(value->qpdf, *this);
}

void
QPDFObject::destroy()
{
    value = QPDF_Destroyed::getInstance();
}

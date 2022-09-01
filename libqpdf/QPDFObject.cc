#include <qpdf/QPDFObject.hh>

#include <qpdf/QPDF.hh>

void
QPDFObject::doResolve()
{
    auto og = value->og;
    QPDF::Resolver::resolve(value->qpdf, og);
}

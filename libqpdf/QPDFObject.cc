#include <qpdf/QPDFObject_private.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDF_Destroyed.hh>

void
QPDFValueProxy::doResolve()
{
    auto og = value->og;
    QPDF::Resolver::resolve(value->qpdf, og);
}

void
QPDFValueProxy::destroy()
{
    value = QPDF_Destroyed::getInstance();
}

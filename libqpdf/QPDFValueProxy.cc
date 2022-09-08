#include <qpdf/QPDFValueProxy.hh>

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
    // See comments in reset() for why this isn't part of reset.
    value = QPDF_Destroyed::getInstance();
}

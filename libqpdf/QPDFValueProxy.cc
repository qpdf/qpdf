#include <qpdf/QPDFValueProxy.hh>

#include <qpdf/QPDF.hh>

void
QPDFValueProxy::doResolve()
{
    auto og = value->og;
    QPDF::Resolver::resolve(value->qpdf, og);
}

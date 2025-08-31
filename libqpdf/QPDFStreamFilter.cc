#include <qpdf/QPDFStreamFilter.hh>

#include <qpdf/QPDFObjectHandle_private.hh>

bool
QPDFStreamFilter::setDecodeParms(QPDFObjectHandle decode_parms)
{
    return decode_parms.null();
}

bool
QPDFStreamFilter::isSpecializedCompression()
{
    return false;
}

bool
QPDFStreamFilter::isLossyCompression()
{
    return false;
}

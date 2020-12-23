#include <qpdf/QPDFStreamFilter.hh>

bool
QPDFStreamFilter::setDecodeParms(QPDFObjectHandle decode_parms)
{
    return decode_parms.isNull();
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

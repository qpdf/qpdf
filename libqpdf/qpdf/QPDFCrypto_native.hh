#ifndef QPDFCRYPTO_NATIVE_HH
#define QPDFCRYPTO_NATIVE_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFCryptoImpl.hh>

class QPDFCrypto_native: public QPDFCryptoImpl
{
  public:
    QPDFCrypto_native() = default;

    QPDF_DLL
    virtual ~QPDFCrypto_native() = default;
};

#endif // QPDFCRYPTO_NATIVE_HH

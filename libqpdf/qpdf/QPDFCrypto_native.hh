#ifndef QPDFCRYPTO_NATIVE_HH
#define QPDFCRYPTO_NATIVE_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/MD5_native.hh>
#include <memory>

class QPDFCrypto_native: public QPDFCryptoImpl
{
  public:
    QPDFCrypto_native() = default;

    QPDF_DLL
    virtual ~QPDFCrypto_native() = default;

    virtual void MD5_init();
    virtual void MD5_update(unsigned char const* data, size_t len);
    virtual void MD5_finalize();
    virtual void MD5_digest(MD5_Digest);

  private:
    std::shared_ptr<MD5_native> md5;
};

#endif // QPDFCRYPTO_NATIVE_HH

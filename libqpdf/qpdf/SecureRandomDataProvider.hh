#ifndef __SECURERANDOMDATAPROVIDER_HH__
#define __SECURERANDOMDATAPROVIDER_HH__

#include <qpdf/RandomDataProvider.hh>
#include <qpdf/DLL.h>

class SecureRandomDataProvider: public RandomDataProvider
{
  public:
    QPDF_DLL
    SecureRandomDataProvider();
    QPDF_DLL
    virtual ~SecureRandomDataProvider();

    QPDF_DLL
    virtual void provideRandomData(unsigned char* data, size_t len);

    QPDF_DLL
    static RandomDataProvider* getInstance();
};

#endif // __SECURERANDOMDATAPROVIDER_HH__

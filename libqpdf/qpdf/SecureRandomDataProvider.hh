#ifndef SECURERANDOMDATAPROVIDER_HH
#define SECURERANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class SecureRandomDataProvider: public RandomDataProvider
{
  public:
    SecureRandomDataProvider() = default;
    virtual ~SecureRandomDataProvider() = default;
    virtual void provideRandomData(unsigned char* data, size_t len);
    static RandomDataProvider* getInstance();
};

#endif // SECURERANDOMDATAPROVIDER_HH

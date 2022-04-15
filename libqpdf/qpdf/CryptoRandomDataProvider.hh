#ifndef CRYPTORANDOMDATAPROVIDER_HH
#define CRYPTORANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class CryptoRandomDataProvider: public RandomDataProvider
{
  public:
    CryptoRandomDataProvider() = default;
    virtual ~CryptoRandomDataProvider() = default;
    virtual void provideRandomData(unsigned char* data, size_t len);
    static RandomDataProvider* getInstance();
};

#endif // CRYPTORANDOMDATAPROVIDER_HH

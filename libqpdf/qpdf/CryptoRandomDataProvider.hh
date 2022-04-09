#ifndef CRYPTORANDOMDATAPROVIDER_HH
#define CRYPTORANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class CryptoRandomDataProvider: public RandomDataProvider
{
  public:
    CryptoRandomDataProvider();
    virtual ~CryptoRandomDataProvider();
    virtual void provideRandomData(unsigned char* data, size_t len);
    static RandomDataProvider* getInstance();
};

#endif // CRYPTORANDOMDATAPROVIDER_HH

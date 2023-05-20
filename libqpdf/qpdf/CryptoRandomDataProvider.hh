#ifndef CRYPTORANDOMDATAPROVIDER_HH
#define CRYPTORANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class CryptoRandomDataProvider: public RandomDataProvider
{
  public:
    CryptoRandomDataProvider() = default;
    ~CryptoRandomDataProvider() override = default;
    void provideRandomData(unsigned char* data, size_t len) override;
    static RandomDataProvider* getInstance();
};

#endif // CRYPTORANDOMDATAPROVIDER_HH

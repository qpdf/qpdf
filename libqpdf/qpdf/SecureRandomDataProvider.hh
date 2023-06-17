#ifndef SECURERANDOMDATAPROVIDER_HH
#define SECURERANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class SecureRandomDataProvider: public RandomDataProvider
{
  public:
    SecureRandomDataProvider() = default;
    ~SecureRandomDataProvider() override = default;
    void provideRandomData(unsigned char* data, size_t len) override;
    static RandomDataProvider* getInstance();
};

#endif // SECURERANDOMDATAPROVIDER_HH

#ifndef INSECURERANDOMDATAPROVIDER_HH
#define INSECURERANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class InsecureRandomDataProvider final: public RandomDataProvider
{
  public:
    InsecureRandomDataProvider() = default;
    ~InsecureRandomDataProvider() final = default;
    void provideRandomData(unsigned char* data, size_t len) final;
    static RandomDataProvider* getInstance();

  private:
    long random();

    bool seeded_random{false};
};

#endif // INSECURERANDOMDATAPROVIDER_HH

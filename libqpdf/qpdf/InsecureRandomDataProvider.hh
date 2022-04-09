#ifndef INSECURERANDOMDATAPROVIDER_HH
#define INSECURERANDOMDATAPROVIDER_HH

#include <qpdf/RandomDataProvider.hh>

class InsecureRandomDataProvider: public RandomDataProvider
{
  public:
    InsecureRandomDataProvider();
    virtual ~InsecureRandomDataProvider();
    virtual void provideRandomData(unsigned char* data, size_t len);
    static RandomDataProvider* getInstance();

  private:
    long random();

    bool seeded_random;
};

#endif // INSECURERANDOMDATAPROVIDER_HH

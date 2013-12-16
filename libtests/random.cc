#include <qpdf/QUtil.hh>
#include <qpdf/qpdf-config.h>
#include <qpdf/InsecureRandomDataProvider.hh>
#include <qpdf/SecureRandomDataProvider.hh>
#include <iostream>

class BogusRandomDataProvider: public RandomDataProvider
{
  public:
    virtual ~BogusRandomDataProvider()
    {
    }
    BogusRandomDataProvider()
    {
    }
    virtual void provideRandomData(unsigned char* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            data[i] = static_cast<unsigned char>(i & 0xff);
        }
    }
};

int main()
{
    RandomDataProvider* orig_rdp = QUtil::getRandomDataProvider();
    long r1 = QUtil::random();
    long r2 = QUtil::random();
    if (r1 == r2)
    {
        std::cout << "fail: two randoms were the same\n";
    }
    InsecureRandomDataProvider irdp;
    irdp.provideRandomData(reinterpret_cast<unsigned char*>(&r1), 4);
    irdp.provideRandomData(reinterpret_cast<unsigned char*>(&r2), 4);
    if (r1 == r2)
    {
        std::cout << "fail: two insecure randoms were the same\n";
    }
#ifndef SKIP_OS_SECURE_RANDOM
    SecureRandomDataProvider srdp;
    srdp.provideRandomData(reinterpret_cast<unsigned char*>(&r1), 4);
    srdp.provideRandomData(reinterpret_cast<unsigned char*>(&r2), 4);
    if (r1 == r2)
    {
        std::cout << "fail: two secure randoms were the same\n";
    }
#endif
    BogusRandomDataProvider brdp;
    QUtil::setRandomDataProvider(&brdp);
    if (QUtil::getRandomDataProvider() != &brdp)
    {
        std::cout << "fail: getRandomDataProvider didn't"
            " return our provider\n";
    }
    r1 = QUtil::random();
    r2 = QUtil::random();
    if (r1 != r2)
    {
        std::cout << "fail: two bogus randoms were different\n";
    }
    unsigned char buf[4];
    QUtil::initializeWithRandomBytes(buf, 4);
    if (! ((buf[0] == 0) &&
           (buf[1] == 1) &&
           (buf[2] == 2) &&
           (buf[3] == 3)))
    {
        std::cout << "fail: bogus random didn't provide correct bytes\n";
    }
    QUtil::setRandomDataProvider(0);
    if (QUtil::getRandomDataProvider() != orig_rdp)
    {
        std::cout << "fail: passing null to setRandomDataProvider "
            "didn't reset the random data provider\n";
    }
    std::cout << "random: end of tests\n";
    return 0;
}

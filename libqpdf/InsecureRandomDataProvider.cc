#include <qpdf/InsecureRandomDataProvider.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/qpdf-config.h>
#include <cstdlib>

InsecureRandomDataProvider::InsecureRandomDataProvider() :
    seeded_random(false)
{
}

void
InsecureRandomDataProvider::provideRandomData(unsigned char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        data[i] = static_cast<unsigned char>((this->random() & 0xff0) >> 4);
    }
}

long
InsecureRandomDataProvider::random()
{
    if (!this->seeded_random) {
        // Seed the random number generator with something simple, but
        // just to be interesting, don't use the unmodified current
        // time.  It would be better if this were a more secure seed.
        unsigned int seed =
            static_cast<unsigned int>(QUtil::get_current_time() ^ 0xcccc);
#ifdef HAVE_RANDOM
        ::srandom(seed);
#else
        srand(seed);
#endif
        this->seeded_random = true;
    }

#ifdef HAVE_RANDOM
    return ::random();
#else
    return rand();
#endif
}

RandomDataProvider*
InsecureRandomDataProvider::getInstance()
{
    static InsecureRandomDataProvider instance;
    return &instance;
}

#include <qpdf/InsecureRandomDataProvider.hh>

#include <qpdf/qpdf-config.h>
#include <chrono>
#include <cstdlib>

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
        // Use the time to seed the insecure random number generator. In this case, the important
        // part is that the value is reasonably unique, not that it's a time. Read a high-resolution
        // clock directly rather than using QUtil::get_current_time since QUtil::get_current_time
        // obeys SOURCE_DATE_EPOCH, which breaks the uniqueness of random values. Note that this
        // random provider is disabled in favor of a secure random source by default.
        auto seed =
            static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
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

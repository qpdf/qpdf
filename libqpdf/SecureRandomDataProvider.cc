#include <qpdf/SecureRandomDataProvider.hh>

#include <qpdf/qpdf-config.h>
#include <qpdf/QUtil.hh>
#ifdef _WIN32
# include <Windows.h>
# include <direct.h>
# include <io.h>
# ifndef SKIP_OS_SECURE_RANDOM
#  include <Wincrypt.h>
# endif
#endif

SecureRandomDataProvider::SecureRandomDataProvider()
{
}

SecureRandomDataProvider::~SecureRandomDataProvider()
{
}

#ifdef SKIP_OS_SECURE_RANDOM

void
SecureRandomDataProvider::provideRandomData(unsigned char* data, size_t len)
{
    throw std::logic_error("SecureRandomDataProvider::provideRandomData called when support was not compiled in");
}

RandomDataProvider*
SecureRandomDataProvider::getInstance()
{
    return 0;
}

#else

#ifdef _WIN32

class WindowsCryptProvider
{
  public:
    WindowsCryptProvider()
    {
        if (!CryptAcquireContext(&crypt_prov,
                                 "Container",
                                 NULL,
                                 PROV_RSA_FULL,
                                 0))
        {
#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#           pragma GCC diagnostic push
#           pragma GCC diagnostic ignored "-Wold-style-cast"
#           pragma GCC diagnostic ignored "-Wsign-compare"
# endif
#endif
            if (GetLastError() == NTE_BAD_KEYSET)
#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#           pragma GCC diagnostic pop
# endif
#endif
            {
                if (! CryptAcquireContext(&crypt_prov,
                                         "Container",
                                         NULL,
                                         PROV_RSA_FULL,
                                         CRYPT_NEWKEYSET))
                {
                    throw std::runtime_error(
                        "unable to acquire crypt context with new keyset");
                }
            }
            else
            {
                throw std::runtime_error("unable to acquire crypt context");
            }
        }
    }
    ~WindowsCryptProvider()
    {
        // Ignore error
        CryptReleaseContext(crypt_prov, 0);
    }

    HCRYPTPROV crypt_prov;
};
#endif

void
SecureRandomDataProvider::provideRandomData(unsigned char* data, size_t len)
{
#if defined(_WIN32)

    // Optimization: make the WindowsCryptProvider static as long as
    // it can be done in a thread-safe fashion.
    WindowsCryptProvider c;
    if (! CryptGenRandom(c.crypt_prov, len, reinterpret_cast<BYTE*>(data)))
    {
        throw std::runtime_error("unable to generate secure random data");
    }

#elif defined(RANDOM_DEVICE)

    // Optimization: wrap the file open and close in a class so that
    // the file is closed in a destructor, then make this static to
    // keep the file handle open.  Only do this if it can be done in a
    // thread-safe fashion.
    FILE* f = QUtil::safe_fopen(RANDOM_DEVICE, "rb");
    size_t fr = fread(data, 1, len, f);
    fclose(f);
    if (fr != len)
    {
        throw std::runtime_error(
            "unable to read " +
            QUtil::int_to_string(len) +
            " bytes from " + std::string(RANDOM_DEVICE));
    }

#else

#  error "Don't know how to generate secure random numbers on this platform.  See random number generation in the top-level README"

#endif
}

RandomDataProvider*
SecureRandomDataProvider::getInstance()
{
    static SecureRandomDataProvider instance;
    return &instance;
}

#endif

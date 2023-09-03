#include <qpdf/SecureRandomDataProvider.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/qpdf-config.h>
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <direct.h>
# include <io.h>
# include <windows.h>
# ifndef SKIP_OS_SECURE_RANDOM
#  include <wincrypt.h>
# endif
#endif

#ifdef SKIP_OS_SECURE_RANDOM

void
SecureRandomDataProvider::provideRandomData(unsigned char* data, size_t len)
{
    throw std::logic_error("SecureRandomDataProvider::provideRandomData called "
                           "when support was not compiled in");
}

RandomDataProvider*
SecureRandomDataProvider::getInstance()
{
    return 0;
}

#else

# ifdef _WIN32

namespace
{
    class WindowsCryptProvider
    {
      public:
        WindowsCryptProvider()
        {
            if (!CryptAcquireContextW(
                    &crypt_prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
                throw std::runtime_error("unable to acquire crypt context: " + getErrorMessage());
            }
        }
        ~WindowsCryptProvider()
        {
            // Ignore error
            CryptReleaseContext(crypt_prov, 0);
        }

        HCRYPTPROV crypt_prov;

      private:
        std::string
        getErrorMessage()
        {
            DWORD errorMessageID = ::GetLastError();
            LPSTR messageBuffer = nullptr;
            size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorMessageID,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&messageBuffer),
                0,
                NULL);
            std::string message(messageBuffer, size);
            LocalFree(messageBuffer);
            return (
                "error number " + QUtil::int_to_string_base(errorMessageID, 16) + ": " + message);
        }
    };
} // namespace
# endif

void
SecureRandomDataProvider::provideRandomData(unsigned char* data, size_t len)
{
# if defined(_WIN32)

    // Optimization: make the WindowsCryptProvider static as long as
    // it can be done in a thread-safe fashion.
    WindowsCryptProvider c;
    if (!CryptGenRandom(c.crypt_prov, static_cast<DWORD>(len), reinterpret_cast<BYTE*>(data))) {
        throw std::runtime_error("unable to generate secure random data");
    }

# else
    static std::unique_ptr<QUtil::FileCloser> random_device = []() {
        FILE* f = fopen("/dev/urandom", "rb");
        if (f == nullptr) {
            f = fopen("/dev/arandom", "rb");
        }
        if (f == nullptr) {
            f = fopen("/dev/random", "rb");
        }
        if (f == nullptr) {
            throw std::runtime_error("unable to find device in /dev for generating random numbers");
        }
        return std::make_unique<QUtil::FileCloser>(f);
    }();

    size_t fr = fread(data, 1, len, random_device->f);
    if (fr != len) {
        throw std::runtime_error(
            "unable to read " + std::to_string(len) + " bytes from random number device");
    }

# endif
}

RandomDataProvider*
SecureRandomDataProvider::getInstance()
{
    static SecureRandomDataProvider instance;
    return &instance;
}

#endif

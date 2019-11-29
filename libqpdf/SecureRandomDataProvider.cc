#include <qpdf/SecureRandomDataProvider.hh>

#include <qpdf/qpdf-config.h>
#include <qpdf/QUtil.hh>
#ifdef _WIN32
# include <windows.h>
# include <direct.h>
# include <io.h>
# ifndef SKIP_OS_SECURE_RANDOM
#  include <wincrypt.h>
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
                                 CRYPT_MACHINE_KEYSET))
        {
#if ((defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406) || \
     defined(__clang__))
#           pragma GCC diagnostic push
#           pragma GCC diagnostic ignored "-Wold-style-cast"
#           pragma GCC diagnostic ignored "-Wsign-compare"
#           pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
            if (GetLastError() == NTE_BAD_KEYSET)
#if ((defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406) || \
     defined(__clang__))
#           pragma GCC diagnostic pop
#endif
            {
                if (! CryptAcquireContext(&crypt_prov,
                                          "Container",
                                          NULL,
                                          PROV_RSA_FULL,
                                          CRYPT_NEWKEYSET|CRYPT_MACHINE_KEYSET))
                {
                    throw std::runtime_error(
                        "unable to acquire crypt context with new keyset: " +
                        getErrorMessage());
                }
            }
            else if (GetLastError() == NTE_EXISTS)
            {
                throw std::runtime_error(
                    "unable to acquire crypt context; The key container already exists, but you are attempting to create it. If a previous attempt to open the key failed with NTE_BAD_KEYSET, it implies that access to the key container is denied.: " +
                    getErrorMessage());
            }
            else if (GetLastError() == NTE_KEYSET_NOT_DEF)
            {
                throw std::runtime_error(
                    "unable to acquire crypt context; The Crypto Service Provider (CSP) may not be set up correctly. Use of Regsvr32.exe on CSP DLLs (Rsabase.dll or Rsaenh.dll) may fix the problem, depending on the provider being used.: " +
                    getErrorMessage());
            }
            else
            {
                throw std::runtime_error(
                    "unable to acquire crypt context: " +
                    getErrorMessage());
            }
        }
    }
    ~WindowsCryptProvider()
    {
        // Ignore error
        CryptReleaseContext(crypt_prov, 0);
    }

    HCRYPTPROV crypt_prov;

  private:
    std::string getErrorMessage()
    {
        DWORD errorMessageID = ::GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer), 0, NULL);
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        return ("error number " +
                QUtil::int_to_string_base(errorMessageID, 16) +
                ": " + message);
    }
};
#endif

void
SecureRandomDataProvider::provideRandomData(unsigned char* data, size_t len)
{
#if defined(_WIN32)

    // Optimization: make the WindowsCryptProvider static as long as
    // it can be done in a thread-safe fashion.
    WindowsCryptProvider c;
    if (! CryptGenRandom(c.crypt_prov, static_cast<DWORD>(len),
                         reinterpret_cast<BYTE*>(data)))
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
            QUtil::uint_to_string(len) +
            " bytes from " + std::string(RANDOM_DEVICE));
    }

#else

#  error "Don't know how to generate secure random numbers on this platform.  See random number generation in the top-level README.md"

#endif
}

RandomDataProvider*
SecureRandomDataProvider::getInstance()
{
    static SecureRandomDataProvider instance;
    return &instance;
}

#endif

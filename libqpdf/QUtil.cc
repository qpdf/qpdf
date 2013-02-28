// Include qpdf-config.h first so off_t is guaranteed to have the right size.
#include <qpdf/qpdf-config.h>

#include <qpdf/QUtil.hh>
#include <qpdf/PointerHolder.hh>

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

std::string
QUtil::int_to_string(long long num, int length)
{
    return int_to_string_base(num, 10, length);
}

std::string
QUtil::int_to_string_base(long long num, int base, int length)
{
    // Backward compatibility -- int_to_string, which calls this
    // function, used to use sprintf with %0*d, so we interpret length
    // such that a negative value appends spaces and a positive value
    // prepends zeroes.
    if (! ((base == 8) || (base == 10) || (base == 16)))
    {
        throw std::logic_error(
            "int_to_string_base called with unsupported base");
    }
    std::ostringstream buf;
    buf << std::setbase(base) << std::nouppercase << num;
    std::string result;
    if ((length > 0) &&
        (buf.str().length() < static_cast<size_t>(length)))
    {
	result.append(length - buf.str().length(), '0');
    }
    result += buf.str();
    if ((length < 0) && (buf.str().length() < static_cast<size_t>(-length)))
    {
	result.append(-length - buf.str().length(), ' ');
    }
    return result;
}

std::string
QUtil::double_to_string(double num, int decimal_places)
{
    // Backward compatibility -- this code used to use sprintf and
    // treated decimal_places <= 0 to mean to use the default, which
    // was six decimal places.  Also sprintf with %*.f interprents the
    // length as fixed point rather than significant figures.
    if (decimal_places <= 0)
    {
        decimal_places = 6;
    }
    std::ostringstream buf;
    buf << std::setprecision(decimal_places) << std::fixed << num;
    return buf.str();
}

long long
QUtil::string_to_ll(char const* str)
{
#ifdef _MSC_VER
    return _strtoi64(str, 0, 10);
#else
    return strtoll(str, 0, 10);
#endif
}

unsigned char*
QUtil::unsigned_char_pointer(std::string const& str)
{
    return reinterpret_cast<unsigned char*>(const_cast<char*>(str.c_str()));
}

unsigned char*
QUtil::unsigned_char_pointer(char const* str)
{
    return reinterpret_cast<unsigned char*>(const_cast<char*>(str));
}

void
QUtil::throw_system_error(std::string const& description)
{
    throw std::runtime_error(description + ": " + strerror(errno)); // XXXX
}

int
QUtil::os_wrapper(std::string const& description, int status)
{
    if (status == -1)
    {
	throw_system_error(description);
    }
    return status;
}

FILE*
QUtil::fopen_wrapper(std::string const& description, FILE* f)
{
    if (f == 0)
    {
	throw_system_error(description);
    }
    return f;
}

int
QUtil::seek(FILE* stream, qpdf_offset_t offset, int whence)
{
#if HAVE_FSEEKO
    return fseeko(stream, static_cast<off_t>(offset), whence);
#elif HAVE_FSEEKO64
    return fseeko64(stream, offset, whence);
#else
# ifdef _MSC_VER
    return _fseeki64(stream, offset, whence);
# else
    return fseek(stream, static_cast<long>(offset), whence);
# endif
#endif
}

qpdf_offset_t
QUtil::tell(FILE* stream)
{
#if HAVE_FSEEKO
    return static_cast<qpdf_offset_t>(ftello(stream));
#elif HAVE_FSEEKO64
    return static_cast<qpdf_offset_t>(ftello64(stream));
#else
# ifdef _MSC_VER
    return _ftelli64(stream);
# else
    return static_cast<qpdf_offset_t>(ftell(stream));
# endif
#endif
}

char*
QUtil::copy_string(std::string const& str)
{
    char* result = new char[str.length() + 1];
    // Use memcpy in case string contains nulls
    result[str.length()] = '\0';
    memcpy(result, str.c_str(), str.length());
    return result;
}

std::string
QUtil::hex_encode(std::string const& input)
{
    std::string result;
    for (unsigned int i = 0; i < input.length(); ++i)
    {
        result += QUtil::int_to_string_base(
            static_cast<int>(static_cast<unsigned char>(input[i])), 16, 2);
    }
    return result;
}

void
QUtil::binary_stdout()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

void
QUtil::binary_stdin()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
}

void
QUtil::setLineBuf(FILE* f)
{
#ifndef _WIN32
    setvbuf(f, reinterpret_cast<char *>(NULL), _IOLBF, 0);
#endif
}

char*
QUtil::getWhoami(char* argv0)
{
#ifdef _WIN32
    char pathsep = '\\';
#else
    char pathsep = '/';
#endif
    char* whoami = 0;
    if ((whoami = strrchr(argv0, pathsep)) == NULL)
    {
	whoami = argv0;
    }
    else
    {
	++whoami;
    }
#ifdef _WIN32
    if ((strlen(whoami) > 4) &&
	(strcmp(whoami + strlen(whoami) - 4, ".exe") == 0))
    {
	whoami[strlen(whoami) - 4] = '\0';
    }
#endif
    return whoami;
}

bool
QUtil::get_env(std::string const& var, std::string* value)
{
    // This was basically ripped out of wxWindows.
#ifdef _WIN32
    // first get the size of the buffer
    DWORD len = ::GetEnvironmentVariable(var.c_str(), NULL, 0);
    if (len == 0)
    {
        // this means that there is no such variable
        return false;
    }

    if (value)
    {
	char* t = new char[len + 1];
        ::GetEnvironmentVariable(var.c_str(), t, len);
	*value = t;
	delete [] t;
    }

    return true;
#else
    char* p = getenv(var.c_str());
    if (p == 0)
    {
        return false;
    }
    if (value)
    {
        *value = p;
    }

    return true;
#endif
}

time_t
QUtil::get_current_time()
{
#ifdef _WIN32
    // The procedure to get local time at this resolution comes from
    // the Microsoft documentation.  It says to convert a SYSTEMTIME
    // to a FILETIME, and to copy the FILETIME to a ULARGE_INTEGER.
    // The resulting number is the number of 100-nanosecond intervals
    // between January 1, 1601 and now.  POSIX threads wants a time
    // based on January 1, 1970, so we adjust by subtracting the
    // number of seconds in that time period from the result we get
    // here.
    SYSTEMTIME sysnow;
    GetSystemTime(&sysnow);
    FILETIME filenow;
    SystemTimeToFileTime(&sysnow, &filenow);
    ULARGE_INTEGER uinow;
    uinow.LowPart = filenow.dwLowDateTime;
    uinow.HighPart = filenow.dwHighDateTime;
    ULONGLONG now = uinow.QuadPart;
    return ((now / 10000000LL) - 11644473600LL);
#else
    return time(0);
#endif
}

std::string
QUtil::toUTF8(unsigned long uval)
{
    std::string result;

    // A UTF-8 encoding of a Unicode value is a single byte for
    // Unicode values <= 127.  For larger values, the first byte of
    // the UTF-8 encoding has '1' as each of its n highest bits and
    // '0' for its (n+1)th highest bit where n is the total number of
    // bytes required.  Subsequent bytes start with '10' and have the
    // remaining 6 bits free for encoding.  For example, an 11-bit
    // Unicode value can be stored in two bytes where the first is
    // 110zzzzz, the second is 10zzzzzz, and the z's represent the
    // remaining bits.

    if (uval > 0x7fffffff)
    {
	throw std::runtime_error("bounds error in QUtil::toUTF8");
    }
    else if (uval < 128)
    {
	result += static_cast<char>(uval);
    }
    else
    {
	unsigned char bytes[7];
	bytes[6] = '\0';
	unsigned char* cur_byte = &bytes[5];

	// maximum value that will fit in the current number of bytes
	unsigned char maxval = 0x3f; // six bits

	while (uval > maxval)
	{
	    // Assign low six bits plus 10000000 to lowest unused
	    // byte position, then shift
	    *cur_byte = static_cast<unsigned char>(0x80 + (uval & 0x3f));
	    uval >>= 6;
	    // Maximum that will fit in high byte now shrinks by one bit
	    maxval >>= 1;
	    // Slide to the left one byte
	    --cur_byte;
	    if (cur_byte < bytes)
	    {
		throw std::logic_error("QUtil::toUTF8: overflow error");
	    }
	}
	// If maxval is k bits long, the high (7 - k) bits of the
	// resulting byte must be high.
	*cur_byte = static_cast<unsigned char>(
            (0xff - (1 + (maxval << 1))) + uval);

	result += reinterpret_cast<char*>(cur_byte);
    }

    return result;
}

long
QUtil::random()
{
    static bool seeded_random = false;
    if (! seeded_random)
    {
	// Seed the random number generator with something simple, but
	// just to be interesting, don't use the unmodified current
	// time.  It would be better if this were a more secure seed.
        QUtil::srandom(QUtil::get_current_time() ^ 0xcccc);
	seeded_random = true;
    }

#ifdef HAVE_RANDOM
    return ::random();
#else
    return rand();
#endif
}

void
QUtil::srandom(unsigned int seed)
{
#ifdef HAVE_RANDOM
    ::srandom(seed);
#else
    srand(seed);
#endif
}

void
QUtil::initializeWithRandomBytes(unsigned char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        data[i] = static_cast<unsigned char>((QUtil::random() & 0xff0) >> 4);
    }
}

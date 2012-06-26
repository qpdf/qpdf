// Include qpdf-config.h first so off_t is gauaranteed to have the right size.
#include <qpdf/qpdf-config.h>

#include <qpdf/QUtil.hh>

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
QUtil::int_to_string(long long num, int fullpad)
{
    // This routine will need to be recompiled if an int can be longer than
    // 49 digits.
    char t[50];

    // -2 or -1 to leave space for the possible negative sign and for NUL...
    if (abs(fullpad) > (int)sizeof(t) - ((num < 0)?2:1))
    {
	throw std::logic_error("Util::int_to_string has been called with "
			       "a padding value greater than its internal "
			       "limit");
    }

#ifdef HAVE_PRINTF_LL
# define PRINTF_LL "ll"
#else
# define PRINTF_LL "l"
#endif
    if (fullpad)
    {
	sprintf(t, "%0*" PRINTF_LL "d", fullpad, num);
    }
    else
    {
	sprintf(t, "%" PRINTF_LL "d", num);
    }
#undef PRINTF_LL

    return std::string(t);
}

std::string
QUtil::double_to_string(double num, int decimal_places)
{
    // This routine will need to be recompiled if a double can be longer than
    // 99 digits.
    char t[100];

    std::string lhs = int_to_string((int)num);

    // lhs.length() gives us the length of the part on the right hand
    // side of the dot + 1 for the dot + decimal_places: total size of
    // the required string.  -1 on the sizeof side to allow for NUL at
    // the end.
    //
    // If decimal_places <= 0, it is as if no precision was provided
    // so trust the buffer is big enough.  The following test will
    // always pass in those cases.
    if (decimal_places + 1 + (int)lhs.length() > (int)sizeof(t) - 1)
    {
	throw std::logic_error("Util::double_to_string has been called with "
			       "a number and a decimal places specification "
			       "that would break an internal limit");
    }

    if (decimal_places)
    {
	sprintf(t, "%.*f", decimal_places, num);
    }
    else
    {
	sprintf(t, "%f", num);
    }
    return std::string(t);
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

void
QUtil::throw_system_error(std::string const& description)
{
    throw std::runtime_error(description + ": " + strerror(errno));
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
QUtil::fseek_off_t(FILE* stream, qpdf_offset_t offset, int whence)
{
#if HAVE_FSEEKO
    return fseeko(stream, (off_t)offset, whence);
#elif HAVE_FSEEKO64
    return fseeko64(stream, offset, whence);
#else
# ifdef _MSC_VER
    return _fseeki64(stream, offset, whence);
# else
    return fseek(stream, (long)offset, whence);
# endif
#endif
}

qpdf_offset_t
QUtil::ftell_off_t(FILE* stream)
{
#if HAVE_FSEEKO
    return (qpdf_offset_t)ftello(stream);
#elif HAVE_FSEEKO64
    return (qpdf_offset_t)ftello64(stream);
#else
# ifdef _MSC_VER
    return _ftelli64(stream);
# else
    return (qpdf_offset_t)ftell(stream);
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
    setvbuf(f, (char *) NULL, _IOLBF, 0);
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
	result += (char)(uval);
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
	    *cur_byte = (unsigned char) (0x80 + (uval & 0x3f));
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
	*cur_byte = (unsigned char)((0xff - (1 + (maxval << 1))) + uval);

	result += (char*)cur_byte;
    }

    return result;
}

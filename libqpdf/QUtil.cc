// Include qpdf-config.h first so off_t is guaranteed to have the right size.
#include <qpdf/qpdf-config.h>

#include <qpdf/QUtil.hh>
#include <qpdf/PointerHolder.hh>
#ifdef USE_INSECURE_RANDOM
# include <qpdf/InsecureRandomDataProvider.hh>
#endif
#include <qpdf/SecureRandomDataProvider.hh>
#include <qpdf/QPDFSystemError.hh>

#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
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
    // was six decimal places.  Also sprintf with %*.f interprets the
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
    errno = 0;
#ifdef _MSC_VER
    long long result = _strtoi64(str, 0, 10);
#else
    long long result = strtoll(str, 0, 10);
#endif
    if (errno == ERANGE)
    {
        throw std::runtime_error(
            std::string("overflow/underflow converting ") + str
            + " to 64-bit integer");
    }
    return result;
}

int
QUtil::string_to_int(char const* str)
{
    errno = 0;
    long long_val = strtol(str, 0, 10);
    if (errno == ERANGE)
    {
        throw std::runtime_error(
            std::string("overflow/underflow converting ") + str
            + " to long integer");
    }
    int result = static_cast<int>(long_val);
    if (static_cast<long>(result) != long_val)
    {
        throw std::runtime_error(
            std::string("overflow/underflow converting ") + str
            + " to integer");
    }
    return result;
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
    throw QPDFSystemError(description, errno);
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
QUtil::safe_fopen(char const* filename, char const* mode)
{
    FILE* f = 0;
#ifdef _MSC_VER
    errno_t err = fopen_s(&f, filename, mode);
    if (err != 0)
    {
        errno = err;
        throw_system_error(std::string("open ") + filename);
    }
#else
    f = fopen_wrapper(std::string("open ") + filename, fopen(filename, mode));
#endif
    return f;
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
# if defined _MSC_VER || defined __BORLANDC__
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
# if defined _MSC_VER || defined __BORLANDC__
    return _ftelli64(stream);
# else
    return static_cast<qpdf_offset_t>(ftell(stream));
# endif
#endif
}

bool
QUtil::same_file(char const* name1, char const* name2)
{
    if ((name1 == 0) || (strlen(name1) == 0) ||
        (name2 == 0) || (strlen(name2) == 0))
    {
        return false;
    }
#ifdef _WIN32
    bool same = false;
# ifndef AVOID_WINDOWS_HANDLE
    HANDLE fh1 = CreateFile(name1, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE fh2 = CreateFile(name2, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    BY_HANDLE_FILE_INFORMATION fi1;
    BY_HANDLE_FILE_INFORMATION fi2;
    if ((fh1 != INVALID_HANDLE_VALUE) &&
        (fh2 != INVALID_HANDLE_VALUE) &&
        GetFileInformationByHandle(fh1, &fi1) &&
        GetFileInformationByHandle(fh2, &fi2) &&
        (fi1.dwVolumeSerialNumber == fi2.dwVolumeSerialNumber) &&
        (fi1.nFileIndexLow == fi2.nFileIndexLow) &&
        (fi1.nFileIndexHigh == fi2.nFileIndexHigh))
    {
        same = true;
    }
    if (fh1 != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fh1);
    }
    if (fh2 != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fh2);
    }
# endif
    return same;
#else
    struct stat st1;
    struct stat st2;
    if ((stat(name1, &st1) == 0) &&
        (stat(name2, &st2) == 0) &&
        (st1.st_ino == st2.st_ino) &&
        (st1.st_dev == st2.st_dev))
    {
        return true;
    }
#endif
    return false;
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
            static_cast<int>(static_cast<unsigned char>(input.at(i))), 16, 2);
    }
    return result;
}

std::string
QUtil::hex_decode(std::string const& input)
{
    std::string result;
    size_t pos = 0;
    for (std::string::const_iterator p = input.begin(); p != input.end(); ++p)
    {
        char ch = *p;
        bool skip = false;
        if ((*p >= 'A') && (*p <= 'F'))
        {
            ch -= 'A';
            ch += 10;
        }
        else if ((*p >= 'a') && (*p <= 'f'))
        {
            ch -= 'a';
            ch += 10;
        }
        else if ((*p >= '0') && (*p <= '9'))
        {
            ch -= '0';
        }
        else
        {
            skip = true;
        }
        if (! skip)
        {
            if (pos == 0)
            {
                result.push_back(ch << 4);
                pos = 1;
            }
            else
            {
                result[result.length()-1] += ch;
                pos = 0;
            }
        }
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
    setvbuf(f, reinterpret_cast<char *>(0), _IOLBF, 0);
#endif
}

char*
QUtil::getWhoami(char* argv0)
{
    char* whoami = 0;
    if (((whoami = strrchr(argv0, '/')) == NULL) &&
        ((whoami = strrchr(argv0, '\\')) == NULL))
    {
	whoami = argv0;
    }
    else
    {
	++whoami;
    }

    if ((strlen(whoami) > 4) &&
	(strcmp(whoami + strlen(whoami) - 4, ".exe") == 0))
    {
	whoami[strlen(whoami) - 4] = '\0';
    }

    return whoami;
}

bool
QUtil::get_env(std::string const& var, std::string* value)
{
    // This was basically ripped out of wxWindows.
#ifdef _WIN32
# ifdef NO_GET_ENVIRONMENT
    return false;
# else
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
# endif
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
	    if (cur_byte <= bytes)
	    {
		throw std::logic_error("QUtil::toUTF8: overflow error");
	    }
	    --cur_byte;
	}
	// If maxval is k bits long, the high (7 - k) bits of the
	// resulting byte must be high.
	*cur_byte = static_cast<unsigned char>(
            (0xff - (1 + (maxval << 1))) + uval);

	result += reinterpret_cast<char*>(cur_byte);
    }

    return result;
}

std::string
QUtil::toUTF16(unsigned long uval)
{
    std::string result;
    if ((uval >= 0xd800) && (uval <= 0xdfff))
    {
        result = "\xff\xfd";
    }
    else if (uval <= 0xffff)
    {
        char out[2];
        out[0] = (uval & 0xff00) >> 8;
        out[1] = (uval & 0xff);
        result = std::string(out, 2);
    }
    else if (uval <= 0x10ffff)
    {
        char out[4];
        uval -= 0x10000;
        unsigned short high = ((uval & 0xffc00) >> 10) + 0xd800;
        unsigned short low = (uval & 0x3ff) + 0xdc00;
        out[0] = (high & 0xff00) >> 8;
        out[1] = (high & 0xff);
        out[2] = (low & 0xff00) >> 8;
        out[3] = (low & 0xff);
        result = std::string(out, 4);
    }
    else
    {
        result = "\xff\xfd";
    }

    return result;
}

// Random data support

long
QUtil::random()
{
    long result = 0L;
    initializeWithRandomBytes(
        reinterpret_cast<unsigned char*>(&result),
        sizeof(result));
    return result;
}

static RandomDataProvider* random_data_provider = 0;

#ifdef USE_INSECURE_RANDOM
static RandomDataProvider* insecure_random_data_provider =
    InsecureRandomDataProvider::getInstance();
#else
static RandomDataProvider* insecure_random_data_provider = 0;
#endif
static RandomDataProvider* secure_random_data_provider =
    SecureRandomDataProvider::getInstance();

static void
initialize_random_data_provider()
{
    if (random_data_provider == 0)
    {
        if (secure_random_data_provider)
        {
            random_data_provider = secure_random_data_provider;
        }
        else if (insecure_random_data_provider)
        {
            random_data_provider = insecure_random_data_provider;
        }
    }
    // QUtil.hh has comments indicating that getRandomDataProvider(),
    // which calls this method, never returns null.
    if (random_data_provider == 0)
    {
        throw std::logic_error("QPDF has no random data provider");
    }
}

void
QUtil::setRandomDataProvider(RandomDataProvider* p)
{
    random_data_provider = p;
}

RandomDataProvider*
QUtil::getRandomDataProvider()
{
    initialize_random_data_provider();
    return random_data_provider;
}

void
QUtil::initializeWithRandomBytes(unsigned char* data, size_t len)
{
    initialize_random_data_provider();
    random_data_provider->provideRandomData(data, len);
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

bool
QUtil::is_hex_digit(char ch)
{
    return (ch && (strchr("0123456789abcdefABCDEF", ch) != 0));
}

bool
QUtil::is_space(char ch)
{
    return (ch && (strchr(" \f\n\r\t\v", ch) != 0));
}

bool
QUtil::is_digit(char ch)
{
    return ((ch >= '0') && (ch <= '9'));
}

bool
QUtil::is_number(char const* p)
{
    // ^[\+\-]?(\.\d*|\d+(\.\d*)?)$
    if (! *p)
    {
        return false;
    }
    if ((*p == '-') || (*p == '+'))
    {
        ++p;
    }
    bool found_dot = false;
    bool found_digit = false;
    for (; *p; ++p)
    {
        if (*p == '.')
        {
            if (found_dot)
            {
                // only one dot
                return false;
            }
            found_dot = true;
        }
        else if (QUtil::is_digit(*p))
        {
            found_digit = true;
        }
        else
        {
            return false;
        }
    }
    return found_digit;
}

std::list<std::string>
QUtil::read_lines_from_file(char const* filename)
{
    std::ifstream in(filename, std::ios_base::binary);
    if (! in.is_open())
    {
        throw_system_error(std::string("open ") + filename);
    }
    std::list<std::string> lines = read_lines_from_file(in);
    in.close();
    return lines;
}

std::list<std::string>
QUtil::read_lines_from_file(std::istream& in)
{
    std::list<std::string> result;
    std::string* buf = 0;

    char c;
    while (in.get(c))
    {
	if (buf == 0)
	{
	    result.push_back("");
	    buf = &(result.back());
	    buf->reserve(80);
	}

	if (buf->capacity() == buf->size())
	{
	    buf->reserve(buf->capacity() * 2);
	}
	if (c == '\n')
	{
            // Remove any carriage return that preceded the
            // newline and discard the newline
            if ((! buf->empty()) && ((*(buf->rbegin())) == '\r'))
            {
                buf->erase(buf->length() - 1);
            }
	    buf = 0;
	}
	else
	{
	    buf->append(1, c);
	}
    }

    return result;
}

int
QUtil::strcasecmp(char const *s1, char const *s2)
{
#ifdef _WIN32
    return _stricmp(s1, s2);
#else
    return ::strcasecmp(s1, s2);
#endif
}

static int maybe_from_end(int num, bool from_end, int max)
{
    if (from_end)
    {
        if (num > max)
        {
            num = 0;
        }
        else
        {
            num = max + 1 - num;
        }
    }
    return num;
}

std::vector<int>
QUtil::parse_numrange(char const* range, int max)
{
    std::vector<int> result;
    char const* p = range;
    try
    {
        std::vector<int> work;
        static int const comma = -1;
        static int const dash = -2;

        enum { st_top,
               st_in_number,
               st_after_number } state = st_top;
        bool last_separator_was_dash = false;
        int cur_number = 0;
        bool from_end = false;
        while (*p)
        {
            char ch = *p;
            if (isdigit(ch))
            {
                if (! ((state == st_top) || (state == st_in_number)))
                {
                    throw std::runtime_error("digit not expected");
                }
                state = st_in_number;
                cur_number *= 10;
                cur_number += (ch - '0');
            }
            else if (ch == 'z')
            {
                // z represents max
                if (! (state == st_top))
                {
                    throw std::runtime_error("z not expected");
                }
                state = st_after_number;
                cur_number = max;
            }
            else if (ch == 'r')
            {
                if (! (state == st_top))
                {
                    throw std::runtime_error("r not expected");
                }
                state = st_in_number;
                from_end = true;
            }
            else if ((ch == ',') || (ch == '-'))
            {
                if (! ((state == st_in_number) || (state == st_after_number)))
                {
                    throw std::runtime_error("unexpected separator");
                }
                cur_number = maybe_from_end(cur_number, from_end, max);
                work.push_back(cur_number);
                cur_number = 0;
                from_end = false;
                if (ch == ',')
                {
                    state = st_top;
                    last_separator_was_dash = false;
                    work.push_back(comma);
                }
                else if (ch == '-')
                {
                    if (last_separator_was_dash)
                    {
                        throw std::runtime_error("unexpected dash");
                    }
                    state = st_top;
                    last_separator_was_dash = true;
                    work.push_back(dash);
                }
            }
            else
            {
                throw std::runtime_error("unexpected character");
            }
            ++p;
        }
        if ((state == st_in_number) || (state == st_after_number))
        {
            cur_number = maybe_from_end(cur_number, from_end, max);
            work.push_back(cur_number);
        }
        else
        {
            throw std::runtime_error("number expected");
        }

        p = 0;
        for (size_t i = 0; i < work.size(); i += 2)
        {
            int num = work.at(i);
            // max == 0 means we don't know the max and are just
            // testing for valid syntax.
            if ((max > 0) && ((num < 1) || (num > max)))
            {
                throw std::runtime_error(
                    "number " + QUtil::int_to_string(num) + " out of range");
            }
            if (i == 0)
            {
                result.push_back(work.at(i));
            }
            else
            {
                int separator = work.at(i-1);
                if (separator == comma)
                {
                    result.push_back(num);
                }
                else if (separator == dash)
                {
                    int lastnum = result.back();
                    if (num > lastnum)
                    {
                        for (int j = lastnum + 1; j <= num; ++j)
                        {
                            result.push_back(j);
                        }
                    }
                    else
                    {
                        for (int j = lastnum - 1; j >= num; --j)
                        {
                            result.push_back(j);
                        }
                    }
                }
                else
                {
                    throw std::logic_error(
                        "INTERNAL ERROR parsing numeric range");
                }
            }
        }
    }
    catch (std::runtime_error const& e)
    {
        std::string message;
        if (p)
        {
            message = "error at * in numeric range " +
                std::string(range, p - range) + "*" + p + ": " + e.what();
        }
        else
        {
            message = "error in numeric range " +
                std::string(range) + ": " + e.what();
        }
        throw std::runtime_error(message);
    }
    return result;
}

enum encoding_e { e_utf16, e_ascii, e_winansi, e_macroman };

static unsigned char
encode_winansi(unsigned long codepoint)
{
    // Use this ugly switch statement to avoid a static, which is not
    // thread-safe.
    unsigned char ch = '\0';
    switch (codepoint)
    {
      case 0x20ac:
        ch = 0x80;
        break;
      case 0x152:
        ch = 0x8c;
        break;
      case 0x160:
        ch = 0x8a;
        break;
      case 0x178:
        ch = 0x9f;
        break;
      case 0x17d:
        ch = 0x8e;
        break;
      case 0x2022:
        ch = 0x95;
        break;
      case 0x2c6:
        ch = 0x88;
        break;
      case 0x2020:
        ch = 0x86;
        break;
      case 0x2021:
        ch = 0x87;
        break;
      case 0x2026:
        ch = 0x85;
        break;
      case 0x2014:
        ch = 0x97;
        break;
      case 0x2013:
        ch = 0x96;
        break;
      case 0x192:
        ch = 0x83;
        break;
      case 0x2039:
        ch = 0x8b;
        break;
      case 0x203a:
        ch = 0x9b;
        break;
      case 0x153:
        ch = 0x9c;
        break;
      case 0x2030:
        ch = 0x89;
        break;
      case 0x201e:
        ch = 0x84;
        break;
      case 0x201c:
        ch = 0x93;
        break;
      case 0x201d:
        ch = 0x94;
        break;
      case 0x2018:
        ch = 0x91;
        break;
      case 0x2019:
        ch = 0x92;
        break;
      case 0x201a:
        ch = 0x82;
        break;
      case 0x161:
        ch = 0x9a;
        break;
      case 0x303:
        ch = 0x98;
        break;
      case 0x2122:
        ch = 0x99;
        break;
      case 0x17e:
        ch = 0x9e;
        break;
      default:
        break;
    }
    return ch;
}

static unsigned char
encode_macroman(unsigned long codepoint)
{
    // Use this ugly switch statement to avoid a static, which is not
    // thread-safe.
    unsigned char ch = '\0';
    switch (codepoint)
    {
      case 0xc6:
        ch = 0xae;
        break;
      case 0xc1:
        ch = 0xe7;
        break;
      case 0xc2:
        ch = 0xe5;
        break;
      case 0xc4:
        ch = 0x80;
        break;
      case 0xc0:
        ch = 0xcb;
        break;
      case 0xc5:
        ch = 0x81;
        break;
      case 0xc3:
        ch = 0xcc;
        break;
      case 0xc7:
        ch = 0x82;
        break;
      case 0xc9:
        ch = 0x83;
        break;
      case 0xca:
        ch = 0xe6;
        break;
      case 0xcb:
        ch = 0xe8;
        break;
      case 0xc8:
        ch = 0xe9;
        break;
      case 0xcd:
        ch = 0xea;
        break;
      case 0xce:
        ch = 0xeb;
        break;
      case 0xcf:
        ch = 0xec;
        break;
      case 0xcc:
        ch = 0xed;
        break;
      case 0xd1:
        ch = 0x84;
        break;
      case 0x152:
        ch = 0xce;
        break;
      case 0xd3:
        ch = 0xee;
        break;
      case 0xd4:
        ch = 0xef;
        break;
      case 0xd6:
        ch = 0x85;
        break;
      case 0xd2:
        ch = 0xf1;
        break;
      case 0xd8:
        ch = 0xaf;
        break;
      case 0xd5:
        ch = 0xcd;
        break;
      case 0xda:
        ch = 0xf2;
        break;
      case 0xdb:
        ch = 0xf3;
        break;
      case 0xdc:
        ch = 0x86;
        break;
      case 0xd9:
        ch = 0xf4;
        break;
      case 0x178:
        ch = 0xd9;
        break;
      case 0xe1:
        ch = 0x87;
        break;
      case 0xe2:
        ch = 0x89;
        break;
      case 0x301:
        ch = 0xab;
        break;
      case 0xe4:
        ch = 0x8a;
        break;
      case 0xe6:
        ch = 0xbe;
        break;
      case 0xe0:
        ch = 0x88;
        break;
      case 0xe5:
        ch = 0x8c;
        break;
      case 0xe3:
        ch = 0x8b;
        break;
      case 0x306:
        ch = 0xf9;
        break;
      case 0x2022:
        ch = 0xa5;
        break;
      case 0x2c7:
        ch = 0xff;
        break;
      case 0xe7:
        ch = 0x8d;
        break;
      case 0x327:
        ch = 0xfc;
        break;
      case 0xa2:
        ch = 0xa2;
        break;
      case 0x2c6:
        ch = 0xf6;
        break;
      case 0xa9:
        ch = 0xa9;
        break;
      case 0xa4:
        ch = 0xdb;
        break;
      case 0x2020:
        ch = 0xa0;
        break;
      case 0x2021:
        ch = 0xe0;
        break;
      case 0xb0:
        ch = 0xa1;
        break;
      case 0x308:
        ch = 0xac;
        break;
      case 0xf7:
        ch = 0xd6;
        break;
      case 0x307:
        ch = 0xfa;
        break;
      case 0x131:
        ch = 0xf5;
        break;
      case 0xe9:
        ch = 0x8e;
        break;
      case 0xea:
        ch = 0x90;
        break;
      case 0xeb:
        ch = 0x91;
        break;
      case 0xe8:
        ch = 0x8f;
        break;
      case 0x2026:
        ch = 0xc9;
        break;
      case 0x2014:
        ch = 0xd1;
        break;
      case 0x2013:
        ch = 0xd0;
        break;
      case 0xa1:
        ch = 0xc1;
        break;
      case 0xfb01:
        ch = 0xde;
        break;
      case 0xfb02:
        ch = 0xdf;
        break;
      case 0x192:
        ch = 0xc4;
        break;
      case 0x2044:
        ch = 0xda;
        break;
      case 0xdf:
        ch = 0xa7;
        break;
      case 0xab:
        ch = 0xc7;
        break;
      case 0xbb:
        ch = 0xc8;
        break;
      case 0x2039:
        ch = 0xdc;
        break;
      case 0x203a:
        ch = 0xdd;
        break;
      case 0x30b:
        ch = 0xfd;
        break;
      case 0xed:
        ch = 0x92;
        break;
      case 0xee:
        ch = 0x94;
        break;
      case 0xef:
        ch = 0x95;
        break;
      case 0xec:
        ch = 0x93;
        break;
      case 0xac:
        ch = 0xc2;
        break;
      case 0x304:
        ch = 0xf8;
        break;
      case 0x3bc:
        ch = 0xb5;
        break;
      case 0xf1:
        ch = 0x96;
        break;
      case 0xf3:
        ch = 0x97;
        break;
      case 0xf4:
        ch = 0x99;
        break;
      case 0xf6:
        ch = 0x9a;
        break;
      case 0x153:
        ch = 0xcf;
        break;
      case 0x328:
        ch = 0xfe;
        break;
      case 0xf2:
        ch = 0x98;
        break;
      case 0x1d43:
        ch = 0xbb;
        break;
      case 0x1d52:
        ch = 0xbc;
        break;
      case 0xf8:
        ch = 0xbf;
        break;
      case 0xf5:
        ch = 0x9b;
        break;
      case 0xb6:
        ch = 0xa6;
        break;
      case 0xb7:
        ch = 0xe1;
        break;
      case 0x2030:
        ch = 0xe4;
        break;
      case 0xb1:
        ch = 0xb1;
        break;
      case 0xbf:
        ch = 0xc0;
        break;
      case 0x201e:
        ch = 0xe3;
        break;
      case 0x201c:
        ch = 0xd2;
        break;
      case 0x201d:
        ch = 0xd3;
        break;
      case 0x2018:
        ch = 0xd4;
        break;
      case 0x2019:
        ch = 0xd5;
        break;
      case 0x201a:
        ch = 0xe2;
        break;
      case 0xae:
        ch = 0xa8;
        break;
      case 0x30a:
        ch = 0xfb;
        break;
      case 0xa7:
        ch = 0xa4;
        break;
      case 0xa3:
        ch = 0xa3;
        break;
      case 0x303:
        ch = 0xf7;
        break;
      case 0x2122:
        ch = 0xaa;
        break;
      case 0xfa:
        ch = 0x9c;
        break;
      case 0xfb:
        ch = 0x9e;
        break;
      case 0xfc:
        ch = 0x9f;
        break;
      case 0xf9:
        ch = 0x9d;
        break;
      case 0xff:
        ch = 0xd8;
        break;
      case 0xa5:
        ch = 0xb4;
        break;
      default:
        break;
    }
    return ch;
}

static std::string
transcode_utf8(std::string const& utf8_val, encoding_e encoding,
               char unknown)
{
    std::string result;
    if (encoding == e_utf16)
    {
        result += "\xfe\xff";
    }
    size_t len = utf8_val.length();
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char ch = static_cast<unsigned char>(utf8_val.at(i));
        if (ch < 128)
        {
            if (encoding == e_utf16)
            {
                result += QUtil::toUTF16(ch);
            }
            else
            {
                result.append(1, ch);
            }
        }
        else
        {
            size_t bytes_needed = 0;
            unsigned bit_check = 0x40;
            unsigned char to_clear = 0x80;
            while (ch & bit_check)
            {
                ++bytes_needed;
                to_clear |= bit_check;
                bit_check >>= 1;
            }

            if (((bytes_needed > 5) || (bytes_needed < 1)) ||
                ((i + bytes_needed) >= len))
            {
                if (encoding == e_utf16)
                {
                    result += "\xff\xfd";
                }
                else
                {
                    result.append(1, unknown);
                }
            }
            else
            {
                unsigned long codepoint = (ch & ~to_clear);
                while (bytes_needed > 0)
                {
                    --bytes_needed;
                    ch = utf8_val.at(++i);
                    if ((ch & 0xc0) != 0x80)
                    {
                        --i;
                        codepoint = 0xfffd;
                        break;
                    }
                    codepoint <<= 6;
                    codepoint += (ch & 0x3f);
                }
                if (encoding == e_utf16)
                {
                    result += QUtil::toUTF16(codepoint);
                }
                else
                {
                    ch = '\0';
                    if (encoding == e_winansi)
                    {
                        if ((codepoint >= 160) && (codepoint < 256))
                        {
                            ch = static_cast<unsigned char>(codepoint & 0xff);
                        }
                        else
                        {
                            ch = encode_winansi(codepoint);
                        }
                    }
                    else if (encoding == e_macroman)
                    {
                        ch = encode_macroman(codepoint);
                    }
                    if (ch == '\0')
                    {
                        ch = static_cast<unsigned char>(unknown);
                    }
                    result.append(1, ch);
                }
            }
        }
    }
    return result;
}

std::string
QUtil::utf8_to_utf16(std::string const& utf8)
{
    return transcode_utf8(utf8, e_utf16, 0);
}

std::string
QUtil::utf8_to_ascii(std::string const& utf8, char unknown_char)
{
    return transcode_utf8(utf8, e_ascii, unknown_char);
}

std::string
QUtil::utf8_to_win_ansi(std::string const& utf8, char unknown_char)
{
    return transcode_utf8(utf8, e_winansi, unknown_char);
}

std::string
QUtil::utf8_to_mac_roman(std::string const& utf8, char unknown_char)
{
    return transcode_utf8(utf8, e_macroman, unknown_char);
}

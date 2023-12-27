// Include qpdf-config.h first so off_t is guaranteed to have the right size.
#include <qpdf/qpdf-config.h>

#include <qpdf/QUtil.hh>

#include <qpdf/CryptoRandomDataProvider.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QTC.hh>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#ifndef QPDF_NO_WCHAR_T
# include <cwchar>
#endif
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <direct.h>
# include <io.h>
# include <windows.h>
#else
# include <sys/stat.h>
# include <unistd.h>
#endif
#ifdef HAVE_MALLOC_INFO
# include <malloc.h>
#endif

// First element is 24
static unsigned short pdf_doc_low_to_unicode[] = {
    0x02d8, // 0x18    BREVE
    0x02c7, // 0x19    CARON
    0x02c6, // 0x1a    MODIFIER LETTER CIRCUMFLEX ACCENT
    0x02d9, // 0x1b    DOT ABOVE
    0x02dd, // 0x1c    DOUBLE ACUTE ACCENT
    0x02db, // 0x1d    OGONEK
    0x02da, // 0x1e    RING ABOVE
    0x02dc, // 0x1f    SMALL TILDE
};
// First element is 127
static unsigned short pdf_doc_to_unicode[] = {
    0xfffd, // 0x7f    UNDEFINED
    0x2022, // 0x80    BULLET
    0x2020, // 0x81    DAGGER
    0x2021, // 0x82    DOUBLE DAGGER
    0x2026, // 0x83    HORIZONTAL ELLIPSIS
    0x2014, // 0x84    EM DASH
    0x2013, // 0x85    EN DASH
    0x0192, // 0x86    SMALL LETTER F WITH HOOK
    0x2044, // 0x87    FRACTION SLASH (solidus)
    0x2039, // 0x88    SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    0x203a, // 0x89    SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    0x2212, // 0x8a    MINUS SIGN
    0x2030, // 0x8b    PER MILLE SIGN
    0x201e, // 0x8c    DOUBLE LOW-9 QUOTATION MARK (quotedblbase)
    0x201c, // 0x8d    LEFT DOUBLE QUOTATION MARK (double quote left)
    0x201d, // 0x8e    RIGHT DOUBLE QUOTATION MARK (quotedblright)
    0x2018, // 0x8f    LEFT SINGLE QUOTATION MARK (quoteleft)
    0x2019, // 0x90    RIGHT SINGLE QUOTATION MARK (quoteright)
    0x201a, // 0x91    SINGLE LOW-9 QUOTATION MARK (quotesinglbase)
    0x2122, // 0x92    TRADE MARK SIGN
    0xfb01, // 0x93    LATIN SMALL LIGATURE FI
    0xfb02, // 0x94    LATIN SMALL LIGATURE FL
    0x0141, // 0x95    LATIN CAPITAL LETTER L WITH STROKE
    0x0152, // 0x96    LATIN CAPITAL LIGATURE OE
    0x0160, // 0x97    LATIN CAPITAL LETTER S WITH CARON
    0x0178, // 0x98    LATIN CAPITAL LETTER Y WITH DIAERESIS
    0x017d, // 0x99    LATIN CAPITAL LETTER Z WITH CARON
    0x0131, // 0x9a    LATIN SMALL LETTER DOTLESS I
    0x0142, // 0x9b    LATIN SMALL LETTER L WITH STROKE
    0x0153, // 0x9c    LATIN SMALL LIGATURE OE
    0x0161, // 0x9d    LATIN SMALL LETTER S WITH CARON
    0x017e, // 0x9e    LATIN SMALL LETTER Z WITH CARON
    0xfffd, // 0x9f    UNDEFINED
    0x20ac, // 0xa0    EURO SIGN
};
static unsigned short win_ansi_to_unicode[] = {
    0x20ac, // 0x80
    0xfffd, // 0x81
    0x201a, // 0x82
    0x0192, // 0x83
    0x201e, // 0x84
    0x2026, // 0x85
    0x2020, // 0x86
    0x2021, // 0x87
    0x02c6, // 0x88
    0x2030, // 0x89
    0x0160, // 0x8a
    0x2039, // 0x8b
    0x0152, // 0x8c
    0xfffd, // 0x8d
    0x017d, // 0x8e
    0xfffd, // 0x8f
    0xfffd, // 0x90
    0x2018, // 0x91
    0x2019, // 0x92
    0x201c, // 0x93
    0x201d, // 0x94
    0x2022, // 0x95
    0x2013, // 0x96
    0x2014, // 0x97
    0x0303, // 0x98
    0x2122, // 0x99
    0x0161, // 0x9a
    0x203a, // 0x9b
    0x0153, // 0x9c
    0xfffd, // 0x9d
    0x017e, // 0x9e
    0x0178, // 0x9f
    0x00a0, // 0xa0
};
static unsigned short mac_roman_to_unicode[] = {
    0x00c4, // 0x80
    0x00c5, // 0x81
    0x00c7, // 0x82
    0x00c9, // 0x83
    0x00d1, // 0x84
    0x00d6, // 0x85
    0x00dc, // 0x86
    0x00e1, // 0x87
    0x00e0, // 0x88
    0x00e2, // 0x89
    0x00e4, // 0x8a
    0x00e3, // 0x8b
    0x00e5, // 0x8c
    0x00e7, // 0x8d
    0x00e9, // 0x8e
    0x00e8, // 0x8f
    0x00ea, // 0x90
    0x00eb, // 0x91
    0x00ed, // 0x92
    0x00ec, // 0x93
    0x00ee, // 0x94
    0x00ef, // 0x95
    0x00f1, // 0x96
    0x00f3, // 0x97
    0x00f2, // 0x98
    0x00f4, // 0x99
    0x00f6, // 0x9a
    0x00f5, // 0x9b
    0x00fa, // 0x9c
    0x00f9, // 0x9d
    0x00fb, // 0x9e
    0x00fc, // 0x9f
    0x2020, // 0xa0
    0x00b0, // 0xa1
    0x00a2, // 0xa2
    0x00a3, // 0xa3
    0x00a7, // 0xa4
    0x2022, // 0xa5
    0x00b6, // 0xa6
    0x00df, // 0xa7
    0x00ae, // 0xa8
    0x00a9, // 0xa9
    0x2122, // 0xaa
    0x0301, // 0xab
    0x0308, // 0xac
    0xfffd, // 0xad
    0x00c6, // 0xae
    0x00d8, // 0xaf
    0xfffd, // 0xb0
    0x00b1, // 0xb1
    0xfffd, // 0xb2
    0xfffd, // 0xb3
    0x00a5, // 0xb4
    0x03bc, // 0xb5
    0xfffd, // 0xb6
    0xfffd, // 0xb7
    0xfffd, // 0xb8
    0xfffd, // 0xb9
    0xfffd, // 0xba
    0x1d43, // 0xbb
    0x1d52, // 0xbc
    0xfffd, // 0xbd
    0x00e6, // 0xbe
    0x00f8, // 0xbf
    0x00bf, // 0xc0
    0x00a1, // 0xc1
    0x00ac, // 0xc2
    0xfffd, // 0xc3
    0x0192, // 0xc4
    0xfffd, // 0xc5
    0xfffd, // 0xc6
    0x00ab, // 0xc7
    0x00bb, // 0xc8
    0x2026, // 0xc9
    0xfffd, // 0xca
    0x00c0, // 0xcb
    0x00c3, // 0xcc
    0x00d5, // 0xcd
    0x0152, // 0xce
    0x0153, // 0xcf
    0x2013, // 0xd0
    0x2014, // 0xd1
    0x201c, // 0xd2
    0x201d, // 0xd3
    0x2018, // 0xd4
    0x2019, // 0xd5
    0x00f7, // 0xd6
    0xfffd, // 0xd7
    0x00ff, // 0xd8
    0x0178, // 0xd9
    0x2044, // 0xda
    0x00a4, // 0xdb
    0x2039, // 0xdc
    0x203a, // 0xdd
    0xfb01, // 0xde
    0xfb02, // 0xdf
    0x2021, // 0xe0
    0x00b7, // 0xe1
    0x201a, // 0xe2
    0x201e, // 0xe3
    0x2030, // 0xe4
    0x00c2, // 0xe5
    0x00ca, // 0xe6
    0x00c1, // 0xe7
    0x00cb, // 0xe8
    0x00c8, // 0xe9
    0x00cd, // 0xea
    0x00ce, // 0xeb
    0x00cf, // 0xec
    0x00cc, // 0xed
    0x00d3, // 0xee
    0x00d4, // 0xef
    0xfffd, // 0xf0
    0x00d2, // 0xf1
    0x00da, // 0xf2
    0x00db, // 0xf3
    0x00d9, // 0xf4
    0x0131, // 0xf5
    0x02c6, // 0xf6
    0x0303, // 0xf7
    0x0304, // 0xf8
    0x0306, // 0xf9
    0x0307, // 0xfa
    0x030a, // 0xfb
    0x0327, // 0xfc
    0x030b, // 0xfd
    0x0328, // 0xfe
    0x02c7, // 0xff
};

static std::map<unsigned long, unsigned char> unicode_to_win_ansi = {
    {0x20ac, 0x80}, {0x201a, 0x82}, {0x192, 0x83},  {0x201e, 0x84}, {0x2026, 0x85}, {0x2020, 0x86},
    {0x2021, 0x87}, {0x2c6, 0x88},  {0x2030, 0x89}, {0x160, 0x8a},  {0x2039, 0x8b}, {0x152, 0x8c},
    {0x17d, 0x8e},  {0x2018, 0x91}, {0x2019, 0x92}, {0x201c, 0x93}, {0x201d, 0x94}, {0x2022, 0x95},
    {0x2013, 0x96}, {0x2014, 0x97}, {0x303, 0x98},  {0x2122, 0x99}, {0x161, 0x9a},  {0x203a, 0x9b},
    {0x153, 0x9c},  {0x17e, 0x9e},  {0x178, 0x9f},  {0xa0, 0xa0},
};
static std::map<unsigned long, unsigned char> unicode_to_mac_roman = {
    {0xc4, 0x80},   {0xc5, 0x81},   {0xc7, 0x82},   {0xc9, 0x83},   {0xd1, 0x84},   {0xd6, 0x85},
    {0xdc, 0x86},   {0xe1, 0x87},   {0xe0, 0x88},   {0xe2, 0x89},   {0xe4, 0x8a},   {0xe3, 0x8b},
    {0xe5, 0x8c},   {0xe7, 0x8d},   {0xe9, 0x8e},   {0xe8, 0x8f},   {0xea, 0x90},   {0xeb, 0x91},
    {0xed, 0x92},   {0xec, 0x93},   {0xee, 0x94},   {0xef, 0x95},   {0xf1, 0x96},   {0xf3, 0x97},
    {0xf2, 0x98},   {0xf4, 0x99},   {0xf6, 0x9a},   {0xf5, 0x9b},   {0xfa, 0x9c},   {0xf9, 0x9d},
    {0xfb, 0x9e},   {0xfc, 0x9f},   {0x2020, 0xa0}, {0xb0, 0xa1},   {0xa2, 0xa2},   {0xa3, 0xa3},
    {0xa7, 0xa4},   {0x2022, 0xa5}, {0xb6, 0xa6},   {0xdf, 0xa7},   {0xae, 0xa8},   {0xa9, 0xa9},
    {0x2122, 0xaa}, {0x301, 0xab},  {0x308, 0xac},  {0xc6, 0xae},   {0xd8, 0xaf},   {0xb1, 0xb1},
    {0xa5, 0xb4},   {0x3bc, 0xb5},  {0x1d43, 0xbb}, {0x1d52, 0xbc}, {0xe6, 0xbe},   {0xf8, 0xbf},
    {0xbf, 0xc0},   {0xa1, 0xc1},   {0xac, 0xc2},   {0x192, 0xc4},  {0xab, 0xc7},   {0xbb, 0xc8},
    {0x2026, 0xc9}, {0xc0, 0xcb},   {0xc3, 0xcc},   {0xd5, 0xcd},   {0x152, 0xce},  {0x153, 0xcf},
    {0x2013, 0xd0}, {0x2014, 0xd1}, {0x201c, 0xd2}, {0x201d, 0xd3}, {0x2018, 0xd4}, {0x2019, 0xd5},
    {0xf7, 0xd6},   {0xff, 0xd8},   {0x178, 0xd9},  {0x2044, 0xda}, {0xa4, 0xdb},   {0x2039, 0xdc},
    {0x203a, 0xdd}, {0xfb01, 0xde}, {0xfb02, 0xdf}, {0x2021, 0xe0}, {0xb7, 0xe1},   {0x201a, 0xe2},
    {0x201e, 0xe3}, {0x2030, 0xe4}, {0xc2, 0xe5},   {0xca, 0xe6},   {0xc1, 0xe7},   {0xcb, 0xe8},
    {0xc8, 0xe9},   {0xcd, 0xea},   {0xce, 0xeb},   {0xcf, 0xec},   {0xcc, 0xed},   {0xd3, 0xee},
    {0xd4, 0xef},   {0xd2, 0xf1},   {0xda, 0xf2},   {0xdb, 0xf3},   {0xd9, 0xf4},   {0x131, 0xf5},
    {0x2c6, 0xf6},  {0x303, 0xf7},  {0x304, 0xf8},  {0x306, 0xf9},  {0x307, 0xfa},  {0x30a, 0xfb},
    {0x327, 0xfc},  {0x30b, 0xfd},  {0x328, 0xfe},  {0x2c7, 0xff},
};
static std::map<unsigned long, unsigned char> unicode_to_pdf_doc = {
    {0x02d8, 0x18}, {0x02c7, 0x19}, {0x02c6, 0x1a}, {0x02d9, 0x1b}, {0x02dd, 0x1c}, {0x02db, 0x1d},
    {0x02da, 0x1e}, {0x02dc, 0x1f}, {0x2022, 0x80}, {0x2020, 0x81}, {0x2021, 0x82}, {0x2026, 0x83},
    {0x2014, 0x84}, {0x2013, 0x85}, {0x0192, 0x86}, {0x2044, 0x87}, {0x2039, 0x88}, {0x203a, 0x89},
    {0x2212, 0x8a}, {0x2030, 0x8b}, {0x201e, 0x8c}, {0x201c, 0x8d}, {0x201d, 0x8e}, {0x2018, 0x8f},
    {0x2019, 0x90}, {0x201a, 0x91}, {0x2122, 0x92}, {0xfb01, 0x93}, {0xfb02, 0x94}, {0x0141, 0x95},
    {0x0152, 0x96}, {0x0160, 0x97}, {0x0178, 0x98}, {0x017d, 0x99}, {0x0131, 0x9a}, {0x0142, 0x9b},
    {0x0153, 0x9c}, {0x0161, 0x9d}, {0x017e, 0x9e}, {0xfffd, 0x9f}, {0x20ac, 0xa0},
};

template <typename T>
static std::string
int_to_string_base_internal(T num, int base, int length)
{
    // Backward compatibility -- int_to_string, which calls this function, used to use sprintf with
    // %0*d, so we interpret length such that a negative value appends spaces and a positive value
    // prepends zeroes.
    if (!((base == 8) || (base == 10) || (base == 16))) {
        throw std::logic_error("int_to_string_base called with unsupported base");
    }
    std::string cvt;
    if (base == 10) {
        // Use the more efficient std::to_string when possible
        cvt = std::to_string(num);
    } else {
        std::ostringstream buf;
        buf.imbue(std::locale::classic());
        buf << std::setbase(base) << std::nouppercase << num;
        cvt = buf.str();
    }
    std::string result;
    int str_length = QIntC::to_int(cvt.length());
    if ((length > 0) && (str_length < length)) {
        result.append(QIntC::to_size(length - str_length), '0');
    }
    result += cvt;
    if ((length < 0) && (str_length < -length)) {
        result.append(QIntC::to_size(-length - str_length), ' ');
    }
    return result;
}

std::string
QUtil::int_to_string(long long num, int length)
{
    return int_to_string_base(num, 10, length);
}

std::string
QUtil::uint_to_string(unsigned long long num, int length)
{
    return uint_to_string_base(num, 10, length);
}

std::string
QUtil::int_to_string_base(long long num, int base, int length)
{
    return int_to_string_base_internal(num, base, length);
}

std::string
QUtil::uint_to_string_base(unsigned long long num, int base, int length)
{
    return int_to_string_base_internal(num, base, length);
}

std::string
QUtil::double_to_string(double num, int decimal_places, bool trim_trailing_zeroes)
{
    // Backward compatibility -- this code used to use sprintf and treated decimal_places <= 0 to
    // mean to use the default, which was six decimal places. Starting in 10.2, we trim trailing
    // zeroes by default.
    if (decimal_places <= 0) {
        decimal_places = 6;
    }
    std::ostringstream buf;
    buf.imbue(std::locale::classic());
    buf << std::setprecision(decimal_places) << std::fixed << num;
    std::string result = buf.str();
    if (trim_trailing_zeroes) {
        while ((result.length() > 1) && (result.back() == '0')) {
            result.pop_back();
        }
        if ((result.length() > 1) && (result.back() == '.')) {
            result.pop_back();
        }
    }
    return result;
}

long long
QUtil::string_to_ll(char const* str)
{
    errno = 0;
#ifdef _MSC_VER
    long long result = _strtoi64(str, 0, 10);
#else
    long long result = strtoll(str, nullptr, 10);
#endif
    if (errno == ERANGE) {
        throw std::range_error(
            std::string("overflow/underflow converting ") + str + " to 64-bit integer");
    }
    return result;
}

int
QUtil::string_to_int(char const* str)
{
    // QIntC::to_int does range checking
    return QIntC::to_int(string_to_ll(str));
}

unsigned long long
QUtil::string_to_ull(char const* str)
{
    char const* p = str;
    while (*p && is_space(*p)) {
        ++p;
    }
    if (*p == '-') {
        throw std::runtime_error(
            std::string("underflow converting ") + str + " to 64-bit unsigned integer");
    }

    errno = 0;
#ifdef _MSC_VER
    unsigned long long result = _strtoui64(str, 0, 10);
#else
    unsigned long long result = strtoull(str, nullptr, 10);
#endif
    if (errno == ERANGE) {
        throw std::runtime_error(
            std::string("overflow converting ") + str + " to 64-bit unsigned integer");
    }
    return result;
}

unsigned int
QUtil::string_to_uint(char const* str)
{
    // QIntC::to_uint does range checking
    return QIntC::to_uint(string_to_ull(str));
}

bool
QUtil::is_long_long(char const* str)
{
    try {
        auto i1 = string_to_ll(str);
        std::string s1 = int_to_string(i1);
        return str == s1;
    } catch (std::exception&) {
        // overflow or other error
    }
    return false;
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
    if (status == -1) {
        throw_system_error(description);
    }
    return status;
}

#ifdef _WIN32
static std::shared_ptr<wchar_t>
win_convert_filename(char const* filename)
{
    // Convert the utf-8 encoded filename argument to wchar_t*. First,
    // convert to utf16, then to wchar_t*. Note that u16 will start
    // with the UTF16 marker, which we skip.
    std::string u16 = QUtil::utf8_to_utf16(filename);
    size_t len = u16.length();
    size_t wlen = (len / 2) - 1;
    auto wfilenamep = QUtil::make_shared_array<wchar_t>(wlen + 1);
    wchar_t* wfilename = wfilenamep.get();
    wfilename[wlen] = 0;
    for (unsigned int i = 2; i < len; i += 2) {
        wfilename[(i / 2) - 1] = static_cast<wchar_t>(
            (static_cast<unsigned char>(u16.at(i)) << 8) +
            static_cast<unsigned char>(u16.at(i + 1)));
    }
    return wfilenamep;
}
#endif

FILE*
QUtil::safe_fopen(char const* filename, char const* mode)
{
    FILE* f = nullptr;
#ifdef _WIN32
    std::shared_ptr<wchar_t> wfilenamep = win_convert_filename(filename);
    wchar_t* wfilename = wfilenamep.get();
    auto wmodep = QUtil::make_shared_array<wchar_t>(strlen(mode) + 1);
    wchar_t* wmode = wmodep.get();
    wmode[strlen(mode)] = 0;
    for (size_t i = 0; i < strlen(mode); ++i) {
        wmode[i] = static_cast<wchar_t>(mode[i]);
    }

# ifdef _MSC_VER
    errno_t err = _wfopen_s(&f, wfilename, wmode);
    if (err != 0) {
        errno = err;
    }
# else
    f = _wfopen(wfilename, wmode);
# endif
    if (f == 0) {
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
    if (f == nullptr) {
        throw_system_error(description);
    }
    return f;
}

bool
QUtil::file_can_be_opened(char const* filename)
{
    try {
        fclose(safe_fopen(filename, "rb"));
        return true;
    } catch (std::runtime_error&) {
        // can't open the file
    }
    return false;
}

int
QUtil::seek(FILE* stream, qpdf_offset_t offset, int whence)
{
#if HAVE_FSEEKO
    return fseeko(stream, QIntC::IntConverter<qpdf_offset_t, off_t>::convert(offset), whence);
#elif HAVE_FSEEKO64
    return fseeko64(stream, offset, whence);
#else
# if defined _MSC_VER || defined __BORLANDC__
    return _fseeki64(stream, offset, whence);
# else
    return fseek(stream, QIntC::to_long(offset), whence);
# endif
#endif
}

qpdf_offset_t
QUtil::tell(FILE* stream)
{
#if HAVE_FSEEKO
    return QIntC::to_offset(ftello(stream));
#elif HAVE_FSEEKO64
    return QIntC::to_offset(ftello64(stream));
#else
# if defined _MSC_VER || defined __BORLANDC__
    return _ftelli64(stream);
# else
    return QIntC::to_offset(ftell(stream));
# endif
#endif
}

bool
QUtil::same_file(char const* name1, char const* name2)
{
    if ((name1 == nullptr) || (strlen(name1) == 0) || (name2 == nullptr) || (strlen(name2) == 0)) {
        return false;
    }
#ifdef _WIN32
    bool same = false;
# ifndef AVOID_WINDOWS_HANDLE
    HANDLE fh1 = CreateFile(
        name1, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE fh2 = CreateFile(
        name2, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    BY_HANDLE_FILE_INFORMATION fi1;
    BY_HANDLE_FILE_INFORMATION fi2;
    if ((fh1 != INVALID_HANDLE_VALUE) && (fh2 != INVALID_HANDLE_VALUE) &&
        GetFileInformationByHandle(fh1, &fi1) && GetFileInformationByHandle(fh2, &fi2) &&
        (fi1.dwVolumeSerialNumber == fi2.dwVolumeSerialNumber) &&
        (fi1.nFileIndexLow == fi2.nFileIndexLow) && (fi1.nFileIndexHigh == fi2.nFileIndexHigh)) {
        same = true;
    }
    if (fh1 != INVALID_HANDLE_VALUE) {
        CloseHandle(fh1);
    }
    if (fh2 != INVALID_HANDLE_VALUE) {
        CloseHandle(fh2);
    }
# endif
    return same;
#else
    struct stat st1;
    struct stat st2;
    if ((stat(name1, &st1) == 0) && (stat(name2, &st2) == 0) && (st1.st_ino == st2.st_ino) &&
        (st1.st_dev == st2.st_dev)) {
        return true;
    }
#endif
    return false;
}

void
QUtil::remove_file(char const* path)
{
#ifdef _WIN32
    std::shared_ptr<wchar_t> wpath = win_convert_filename(path);
    os_wrapper(std::string("remove ") + path, _wunlink(wpath.get()));
#else
    os_wrapper(std::string("remove ") + path, unlink(path));
#endif
}

void
QUtil::rename_file(char const* oldname, char const* newname)
{
#ifdef _WIN32
    try {
        remove_file(newname);
    } catch (QPDFSystemError&) {
        // ignore
    }
    std::shared_ptr<wchar_t> wold = win_convert_filename(oldname);
    std::shared_ptr<wchar_t> wnew = win_convert_filename(newname);
    os_wrapper(std::string("rename ") + oldname + " " + newname, _wrename(wold.get(), wnew.get()));
#else
    os_wrapper(std::string("rename ") + oldname + " " + newname, rename(oldname, newname));
#endif
}

void
QUtil::pipe_file(char const* filename, Pipeline* p)
{
    // Exercised in test suite by testing file_provider.
    FILE* f = safe_fopen(filename, "rb");
    FileCloser fc(f);
    size_t len = 0;
    int constexpr size = 8192;
    unsigned char buf[size];
    while ((len = fread(buf, 1, size, f)) > 0) {
        p->write(buf, len);
    }
    p->finish();
    if (ferror(f)) {
        throw std::runtime_error(std::string("failure reading file ") + filename);
    }
}

std::function<void(Pipeline*)>
QUtil::file_provider(std::string const& filename)
{
    return [filename](Pipeline* p) { pipe_file(filename.c_str(), p); };
}

std::string
QUtil::path_basename(std::string const& filename)
{
#ifdef _WIN32
    char const* pathsep = "/\\";
#else
    char const* pathsep = "/";
#endif
    std::string last = filename;
    auto len = last.length();
    while (len > 1) {
        auto pos = last.find_last_of(pathsep);
        if (pos == len - 1) {
            last.pop_back();
            --len;
        } else if (pos == std::string::npos) {
            break;
        } else {
            last = last.substr(pos + 1);
            break;
        }
    }
    return last;
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

std::shared_ptr<char>
QUtil::make_shared_cstr(std::string const& str)
{
    auto result = QUtil::make_shared_array<char>(str.length() + 1);
    // Use memcpy in case string contains nulls
    result.get()[str.length()] = '\0';
    memcpy(result.get(), str.c_str(), str.length());
    return result;
}

std::unique_ptr<char[]>
QUtil::make_unique_cstr(std::string const& str)
{
    auto result = std::make_unique<char[]>(str.length() + 1);
    // Use memcpy in case string contains nulls
    result.get()[str.length()] = '\0';
    memcpy(result.get(), str.c_str(), str.length());
    return result;
}

std::string
QUtil::hex_encode(std::string const& input)
{
    static auto constexpr hexchars = "0123456789abcdef";
    std::string result;
    result.reserve(2 * input.length());
    for (const char c: input) {
        result += hexchars[static_cast<unsigned char>(c) >> 4];
        result += hexchars[c & 0x0f];
    }
    return result;
}

std::string
QUtil::hex_decode(std::string const& input)
{
    std::string result;
    // We know result.size() <= 0.5 * input.size() + 1. However, reserving string space for this
    // upper bound has a negative impact.
    bool first = true;
    char decoded;
    for (auto ch: input) {
        ch = hex_decode_char(ch);
        if (ch < '\20') {
            if (first) {
                decoded = static_cast<char>(ch << 4);
                first = false;
            } else {
                result.push_back(decoded | ch);
                first = true;
            }
        }
    }
    if (!first) {
        result.push_back(decoded);
    }
    return result;
}

void
QUtil::binary_stdout()
{
#if defined(_WIN32) && defined(__BORLANDC__)
    setmode(_fileno(stdout), _O_BINARY);
#elif defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

void
QUtil::binary_stdin()
{
#if defined(_WIN32) && defined(__BORLANDC__)
    setmode(_fileno(stdin), _O_BINARY);
#elif defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
#endif
}

void
QUtil::setLineBuf(FILE* f)
{
#ifndef _WIN32
    setvbuf(f, reinterpret_cast<char*>(0), _IOLBF, 0);
#endif
}

char*
QUtil::getWhoami(char* argv0)
{
    char* whoami = nullptr;
    if (((whoami = strrchr(argv0, '/')) == nullptr) &&
        ((whoami = strrchr(argv0, '\\')) == nullptr)) {
        whoami = argv0;
    } else {
        ++whoami;
    }

    if ((strlen(whoami) > 4) && (strcmp(whoami + strlen(whoami) - 4, ".exe") == 0)) {
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
    if (len == 0) {
        // this means that there is no such variable
        return false;
    }

    if (value) {
        auto t = QUtil::make_shared_array<char>(len + 1);
        ::GetEnvironmentVariable(var.c_str(), t.get(), len);
        *value = t.get();
    }

    return true;
# endif
#else
    char* p = getenv(var.c_str());
    if (p == nullptr) {
        return false;
    }
    if (value) {
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
    return static_cast<time_t>((now / 10000000ULL) - 11644473600ULL);
#else
    return time(nullptr);
#endif
}

QUtil::QPDFTime
QUtil::get_current_qpdf_time()
{
#ifdef _WIN32
    SYSTEMTIME ltime;
    GetLocalTime(&ltime);
    TIME_ZONE_INFORMATION tzinfo;
    GetTimeZoneInformation(&tzinfo);
    return QPDFTime(
        static_cast<int>(ltime.wYear),
        static_cast<int>(ltime.wMonth),
        static_cast<int>(ltime.wDay),
        static_cast<int>(ltime.wHour),
        static_cast<int>(ltime.wMinute),
        static_cast<int>(ltime.wSecond),
        // tzinfo.Bias is minutes before UTC
        static_cast<int>(tzinfo.Bias));
#else
    struct tm ltime;
    time_t now = time(nullptr);
    tzset();
# ifdef HAVE_LOCALTIME_R
    localtime_r(&now, &ltime);
# else
    ltime = *localtime(&now);
# endif
# if HAVE_TM_GMTOFF
    // tm_gmtoff is seconds after UTC
    int tzoff = -static_cast<int>(ltime.tm_gmtoff / 60);
# elif HAVE_EXTERN_LONG_TIMEZONE
    // timezone is seconds before UTC, not adjusted for daylight saving time
    int tzoff = static_cast<int>(timezone / 60);
# else
    // Don't know how to get timezone on this platform
    int tzoff = 0;
# endif
    return {
        static_cast<int>(ltime.tm_year + 1900),
        static_cast<int>(ltime.tm_mon + 1),
        static_cast<int>(ltime.tm_mday),
        static_cast<int>(ltime.tm_hour),
        static_cast<int>(ltime.tm_min),
        static_cast<int>(ltime.tm_sec),
        tzoff};
#endif
}

std::string
QUtil::qpdf_time_to_pdf_time(QPDFTime const& qtm)
{
    std::string tz_offset;
    int t = qtm.tz_delta;
    if (t == 0) {
        tz_offset = "Z";
    } else {
        if (t < 0) {
            t = -t;
            tz_offset += "+";
        } else {
            tz_offset += "-";
        }
        tz_offset += QUtil::int_to_string(t / 60, 2) + "'" + QUtil::int_to_string(t % 60, 2) + "'";
    }
    return (
        "D:" + QUtil::int_to_string(qtm.year, 4) + QUtil::int_to_string(qtm.month, 2) +
        QUtil::int_to_string(qtm.day, 2) + QUtil::int_to_string(qtm.hour, 2) +
        QUtil::int_to_string(qtm.minute, 2) + QUtil::int_to_string(qtm.second, 2) + tz_offset);
}

std::string
QUtil::qpdf_time_to_iso8601(QPDFTime const& qtm)
{
    std::string tz_offset;
    int t = qtm.tz_delta;
    if (t == 0) {
        tz_offset = "Z";
    } else {
        if (t < 0) {
            t = -t;
            tz_offset += "+";
        } else {
            tz_offset += "-";
        }
        tz_offset += QUtil::int_to_string(t / 60, 2) + ":" + QUtil::int_to_string(t % 60, 2);
    }
    return (
        QUtil::int_to_string(qtm.year, 4) + "-" + QUtil::int_to_string(qtm.month, 2) + "-" +
        QUtil::int_to_string(qtm.day, 2) + "T" + QUtil::int_to_string(qtm.hour, 2) + ":" +
        QUtil::int_to_string(qtm.minute, 2) + ":" + QUtil::int_to_string(qtm.second, 2) +
        tz_offset);
}

bool
QUtil::pdf_time_to_qpdf_time(std::string const& str, QPDFTime* qtm)
{
    static std::regex pdf_date("^D:([0-9]{4})([0-9]{2})([0-9]{2})"
                               "([0-9]{2})([0-9]{2})([0-9]{2})"
                               "(?:(Z?)|([\\+\\-])([0-9]{2})'([0-9]{2})')$");
    std::smatch m;
    if (!std::regex_match(str, m, pdf_date)) {
        return false;
    }
    int tz_delta = 0;
    auto to_i = [](std::string const& s) { return QUtil::string_to_int(s.c_str()); };

    if (m[8] != "") {
        tz_delta = ((to_i(m[9]) * 60) + to_i(m[10]));
        if (m[8] == "+") {
            tz_delta = -tz_delta;
        }
    }
    if (qtm) {
        *qtm = QPDFTime(
            to_i(m[1]), to_i(m[2]), to_i(m[3]), to_i(m[4]), to_i(m[5]), to_i(m[6]), tz_delta);
    }
    return true;
}

bool
QUtil::pdf_time_to_iso8601(std::string const& pdf_time, std::string& iso8601)
{
    QPDFTime qtm;
    if (pdf_time_to_qpdf_time(pdf_time, &qtm)) {
        iso8601 = qpdf_time_to_iso8601(qtm);
        return true;
    }
    return false;
}

std::string
QUtil::toUTF8(unsigned long uval)
{
    std::string result;

    // A UTF-8 encoding of a Unicode value is a single byte for Unicode values <= 127.  For larger
    // values, the first byte of the UTF-8 encoding has '1' as each of its n highest bits and '0'
    // for its (n+1)th highest bit where n is the total number of bytes required.  Subsequent bytes
    // start with '10' and have the remaining 6 bits free for encoding.  For example, an 11-bit
    // Unicode value can be stored in two bytes where the first is 110zzzzz, the second is 10zzzzzz,
    // and the z's represent the remaining bits.

    if (uval > 0x7fffffff) {
        throw std::runtime_error("bounds error in QUtil::toUTF8");
    } else if (uval < 128) {
        result += static_cast<char>(uval);
    } else {
        unsigned char bytes[7];
        bytes[6] = '\0';
        unsigned char* cur_byte = &bytes[5];

        // maximum value that will fit in the current number of bytes
        unsigned char maxval = 0x3f; // six bits

        while (uval > QIntC::to_ulong(maxval)) {
            // Assign low six bits plus 10000000 to lowest unused byte position, then shift
            *cur_byte = static_cast<unsigned char>(0x80 + (uval & 0x3f));
            uval >>= 6;
            // Maximum that will fit in high byte now shrinks by one bit
            maxval = static_cast<unsigned char>(maxval >> 1);
            // Slide to the left one byte
            if (cur_byte <= bytes) {
                throw std::logic_error("QUtil::toUTF8: overflow error");
            }
            --cur_byte;
        }
        // If maxval is k bits long, the high (7 - k) bits of the resulting byte must be high.
        *cur_byte = static_cast<unsigned char>(QIntC::to_ulong(0xff - (1 + (maxval << 1))) + uval);

        result += reinterpret_cast<char*>(cur_byte);
    }

    return result;
}

std::string
QUtil::toUTF16(unsigned long uval)
{
    std::string result;
    if ((uval >= 0xd800) && (uval <= 0xdfff)) {
        result = "\xff\xfd";
    } else if (uval <= 0xffff) {
        char out[2];
        out[0] = static_cast<char>((uval & 0xff00) >> 8);
        out[1] = static_cast<char>(uval & 0xff);
        result = std::string(out, 2);
    } else if (uval <= 0x10ffff) {
        char out[4];
        uval -= 0x10000;
        unsigned short high = static_cast<unsigned short>(((uval & 0xffc00) >> 10) + 0xd800);
        unsigned short low = static_cast<unsigned short>((uval & 0x3ff) + 0xdc00);
        out[0] = static_cast<char>((high & 0xff00) >> 8);
        out[1] = static_cast<char>(high & 0xff);
        out[2] = static_cast<char>((low & 0xff00) >> 8);
        out[3] = static_cast<char>(low & 0xff);
        result = std::string(out, 4);
    } else {
        result = "\xff\xfd";
    }

    return result;
}

// Random data support

namespace
{
    class RandomDataProviderProvider
    {
      public:
        RandomDataProviderProvider();
        void setProvider(RandomDataProvider*);
        RandomDataProvider* getProvider();

      private:
        RandomDataProvider* default_provider;
        RandomDataProvider* current_provider{nullptr};
    };
} // namespace

RandomDataProviderProvider::RandomDataProviderProvider() :
    default_provider(CryptoRandomDataProvider::getInstance())
{
    this->current_provider = default_provider;
}

RandomDataProvider*
RandomDataProviderProvider::getProvider()
{
    return this->current_provider;
}

void
RandomDataProviderProvider::setProvider(RandomDataProvider* p)
{
    this->current_provider = p ? p : this->default_provider;
}

static RandomDataProviderProvider*
getRandomDataProviderProvider()
{
    // Thread-safe static initializer
    static RandomDataProviderProvider rdpp;
    return &rdpp;
}

void
QUtil::setRandomDataProvider(RandomDataProvider* p)
{
    getRandomDataProviderProvider()->setProvider(p);
}

RandomDataProvider*
QUtil::getRandomDataProvider()
{
    return getRandomDataProviderProvider()->getProvider();
}

void
QUtil::initializeWithRandomBytes(unsigned char* data, size_t len)
{
    getRandomDataProvider()->provideRandomData(data, len);
}

long
QUtil::random()
{
    long result = 0L;
    initializeWithRandomBytes(reinterpret_cast<unsigned char*>(&result), sizeof(result));
    return result;
}

void
QUtil::read_file_into_memory(char const* filename, std::shared_ptr<char>& file_buf, size_t& size)
{
    FILE* f = safe_fopen(filename, "rb");
    FileCloser fc(f);
    fseek(f, 0, SEEK_END);
    size = QIntC::to_size(QUtil::tell(f));
    fseek(f, 0, SEEK_SET);
    file_buf = QUtil::make_shared_array<char>(size);
    char* buf_p = file_buf.get();
    size_t bytes_read = 0;
    size_t len = 0;
    while ((len = fread(buf_p + bytes_read, 1, size - bytes_read, f)) > 0) {
        bytes_read += len;
    }
    if (bytes_read != size) {
        if (ferror(f)) {
            throw std::runtime_error(
                std::string("failure reading file ") + filename + " into memory: read " +
                uint_to_string(bytes_read) + "; wanted " + uint_to_string(size));
        } else {
            throw std::runtime_error(
                std::string("premature eof reading file ") + filename + " into memory: read " +
                uint_to_string(bytes_read) + "; wanted " + uint_to_string(size));
        }
    }
}

std::string
QUtil::read_file_into_string(char const* filename)
{
    FILE* f = safe_fopen(filename, "rb");
    FileCloser fc(f);
    return read_file_into_string(f, filename);
}

std::string
QUtil::read_file_into_string(FILE* f, std::string_view filename)
{
    fseek(f, 0, SEEK_END);
    auto o_size = QUtil::tell(f);
    if (o_size >= 0) {
        // Seekable file
        auto size = QIntC::to_size(o_size);
        fseek(f, 0, SEEK_SET);
        std::string result(size, '\0');
        if (auto n_read = fread(result.data(), 1, size, f); n_read != size) {
            if (ferror(f)) {
                throw std::runtime_error(
                    std::string("failure reading file ") + std::string(filename) +
                    " into memory: read " + uint_to_string(n_read) + "; wanted " +
                    uint_to_string(size));
            } else {
                throw std::runtime_error(
                    std::string("premature eof reading file ") + std::string(filename) +
                    " into memory: read " + uint_to_string(n_read) + "; wanted " +
                    uint_to_string(size));
            }
        }
        return result;
    } else {
        // Pipe or other non-seekable file
        size_t buf_size = 8192;
        auto n_read = buf_size;
        std::string buffer(buf_size, '\0');
        std::string result;
        while (n_read == buf_size) {
            n_read = fread(buffer.data(), 1, buf_size, f);
            buffer.erase(n_read);
            result.append(buffer);
        }
        if (ferror(f)) {
            throw std::runtime_error(
                std::string("failure reading file ") + std::string(filename) + " into memory");
        }
        return result;
    }
}

static bool
read_char_from_FILE(char& ch, FILE* f)
{
    auto len = fread(&ch, 1, 1, f);
    if (len == 0) {
        if (ferror(f)) {
            throw std::runtime_error("failure reading character from file");
        }
        return false;
    }
    return true;
}

std::list<std::string>
QUtil::read_lines_from_file(char const* filename, bool preserve_eol)
{
    std::list<std::string> lines;
    FILE* f = safe_fopen(filename, "rb");
    FileCloser fc(f);
    auto next_char = [&f](char& ch) { return read_char_from_FILE(ch, f); };
    read_lines_from_file(next_char, lines, preserve_eol);
    return lines;
}

std::list<std::string>
QUtil::read_lines_from_file(std::istream& in, bool preserve_eol)
{
    std::list<std::string> lines;
    auto next_char = [&in](char& ch) { return (in.get(ch)) ? true : false; };
    read_lines_from_file(next_char, lines, preserve_eol);
    return lines;
}

std::list<std::string>
QUtil::read_lines_from_file(FILE* f, bool preserve_eol)
{
    std::list<std::string> lines;
    auto next_char = [&f](char& ch) { return read_char_from_FILE(ch, f); };
    read_lines_from_file(next_char, lines, preserve_eol);
    return lines;
}

void
QUtil::read_lines_from_file(
    std::function<bool(char&)> next_char, std::list<std::string>& lines, bool preserve_eol)
{
    std::string* buf = nullptr;
    char c;
    while (next_char(c)) {
        if (buf == nullptr) {
            lines.emplace_back("");
            buf = &(lines.back());
            buf->reserve(80);
        }

        if (buf->capacity() == buf->size()) {
            buf->reserve(buf->capacity() * 2);
        }
        if (c == '\n') {
            if (preserve_eol) {
                buf->append(1, c);
            } else {
                // Remove any carriage return that preceded the newline and discard the newline
                if ((!buf->empty()) && ((*(buf->rbegin())) == '\r')) {
                    buf->erase(buf->length() - 1);
                }
            }
            buf = nullptr;
        } else {
            buf->append(1, c);
        }
    }
}

int
QUtil::str_compare_nocase(char const* s1, char const* s2)
{
#if defined(_WIN32) && defined(__BORLANDC__)
    return stricmp(s1, s2);
#elif defined(_WIN32)
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

static int
maybe_from_end(int num, bool from_end, int max)
{
    if (from_end) {
        if (num > max) {
            num = 0;
        } else {
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
    try {
        std::vector<int> work;
        static int const comma = -1;
        static int const dash = -2;
        size_t start_idx = 0;
        size_t skip = 1;

        enum { st_top, st_in_number, st_after_number } state = st_top;
        bool last_separator_was_dash = false;
        int cur_number = 0;
        bool from_end = false;
        while (*p) {
            char ch = *p;
            if (isdigit(ch)) {
                if (!((state == st_top) || (state == st_in_number))) {
                    throw std::runtime_error("digit not expected");
                }
                state = st_in_number;
                cur_number *= 10;
                cur_number += (ch - '0');
            } else if (ch == 'z') {
                // z represents max
                if (!(state == st_top)) {
                    throw std::runtime_error("z not expected");
                }
                state = st_after_number;
                cur_number = max;
            } else if (ch == 'r') {
                if (!(state == st_top)) {
                    throw std::runtime_error("r not expected");
                }
                state = st_in_number;
                from_end = true;
            } else if ((ch == ',') || (ch == '-')) {
                if (!((state == st_in_number) || (state == st_after_number))) {
                    throw std::runtime_error("unexpected separator");
                }
                cur_number = maybe_from_end(cur_number, from_end, max);
                work.push_back(cur_number);
                cur_number = 0;
                from_end = false;
                if (ch == ',') {
                    state = st_top;
                    last_separator_was_dash = false;
                    work.push_back(comma);
                } else if (ch == '-') {
                    if (last_separator_was_dash) {
                        throw std::runtime_error("unexpected dash");
                    }
                    state = st_top;
                    last_separator_was_dash = true;
                    work.push_back(dash);
                }
            } else if (ch == ':') {
                if (!((state == st_in_number) || (state == st_after_number))) {
                    throw std::runtime_error("unexpected colon");
                }
                break;
            } else {
                throw std::runtime_error("unexpected character");
            }
            ++p;
        }
        if ((state == st_in_number) || (state == st_after_number)) {
            cur_number = maybe_from_end(cur_number, from_end, max);
            work.push_back(cur_number);
        } else {
            throw std::runtime_error("number expected");
        }
        if (*p == ':') {
            if (strcmp(p, ":odd") == 0) {
                skip = 2;
            } else if (strcmp(p, ":even") == 0) {
                skip = 2;
                start_idx = 1;
            } else {
                throw std::runtime_error("unexpected even/odd modifier");
            }
        }

        p = nullptr;
        for (size_t i = 0; i < work.size(); i += 2) {
            int num = work.at(i);
            // max == 0 means we don't know the max and are just testing for valid syntax.
            if ((max > 0) && ((num < 1) || (num > max))) {
                throw std::runtime_error("number " + QUtil::int_to_string(num) + " out of range");
            }
            if (i == 0) {
                result.push_back(work.at(i));
            } else {
                int separator = work.at(i - 1);
                if (separator == comma) {
                    result.push_back(num);
                } else if (separator == dash) {
                    int lastnum = result.back();
                    if (num > lastnum) {
                        for (int j = lastnum + 1; j <= num; ++j) {
                            result.push_back(j);
                        }
                    } else {
                        for (int j = lastnum - 1; j >= num; --j) {
                            result.push_back(j);
                        }
                    }
                } else {
                    throw std::logic_error("INTERNAL ERROR parsing numeric range");
                }
            }
        }
        if ((start_idx > 0) || (skip != 1)) {
            auto t = result;
            result.clear();
            for (size_t i = start_idx; i < t.size(); i += skip) {
                result.push_back(t.at(i));
            }
        }
    } catch (std::runtime_error const& e) {
        std::string message;
        if (p) {
            message = "error at * in numeric range " +
                std::string(range, QIntC::to_size(p - range)) + "*" + p + ": " + e.what();
        } else {
            message = "error in numeric range " + std::string(range) + ": " + e.what();
        }
        throw std::runtime_error(message);
    }
    return result;
}

enum encoding_e { e_utf16, e_ascii, e_winansi, e_macroman, e_pdfdoc };

static unsigned char
encode_winansi(unsigned long codepoint)
{
    auto i = unicode_to_win_ansi.find(codepoint);
    if (i != unicode_to_win_ansi.end()) {
        return i->second;
    }
    return '\0';
}

static unsigned char
encode_macroman(unsigned long codepoint)
{
    auto i = unicode_to_mac_roman.find(codepoint);
    if (i != unicode_to_mac_roman.end()) {
        return i->second;
    }
    return '\0';
}

static unsigned char
encode_pdfdoc(unsigned long codepoint)
{
    auto i = unicode_to_pdf_doc.find(codepoint);
    if (i != unicode_to_pdf_doc.end()) {
        return i->second;
    }
    return '\0';
}

unsigned long
QUtil::get_next_utf8_codepoint(std::string const& utf8_val, size_t& pos, bool& error)
{
    auto o_pos = pos;
    size_t len = utf8_val.length();
    unsigned char ch = static_cast<unsigned char>(utf8_val.at(pos++));
    error = false;
    if (ch < 128) {
        return static_cast<unsigned long>(ch);
    }

    size_t bytes_needed = 0;
    unsigned bit_check = 0x40;
    unsigned char to_clear = 0x80;
    while (ch & bit_check) {
        ++bytes_needed;
        to_clear = static_cast<unsigned char>(to_clear | bit_check);
        bit_check >>= 1;
    }
    if (((bytes_needed > 5) || (bytes_needed < 1)) || ((pos + bytes_needed) > len)) {
        error = true;
        return 0xfffd;
    }

    auto codepoint = static_cast<unsigned long>(ch & ~to_clear);
    while (bytes_needed > 0) {
        --bytes_needed;
        ch = static_cast<unsigned char>(utf8_val.at(pos++));
        if ((ch & 0xc0) != 0x80) {
            --pos;
            error = true;
            return 0xfffd;
        }
        codepoint <<= 6;
        codepoint += (ch & 0x3f);
    }
    unsigned long lower_bound = 0;
    switch (pos - o_pos) {
    case 2:
        lower_bound = 1 << 7;
        break;
    case 3:
        lower_bound = 1 << 11;
        break;
    case 4:
        lower_bound = 1 << 16;
        break;
    case 5:
        lower_bound = 1 << 12;
        break;
    case 6:
        lower_bound = 1 << 26;
        break;
    default:
        lower_bound = 0;
    }

    if (lower_bound > 0 && codepoint < lower_bound) {
        // Too many bytes were used, but return whatever character was encoded.
        error = true;
    }
    return codepoint;
}

static bool
transcode_utf8(std::string const& utf8_val, std::string& result, encoding_e encoding, char unknown)
{
    bool okay = true;
    result.clear();
    size_t len = utf8_val.length();
    switch (encoding) {
    case e_utf16:
        result += "\xfe\xff";
        break;
    case e_pdfdoc:
        // We need to avoid having the result start with something that will be interpreted as
        // UTF-16 or UTF-8, meaning we can't end up with a string that starts with "fe ff",
        // (UTF-16-BE) "ff fe" (UTF-16-LE, not officially part of the PDF spec, but recognized by
        // most readers including qpdf), or "ef bb bf" (UTF-8). It's more efficient to check the
        // input string to see if it will map to one of those sequences than to check the output
        // string since all cases start with the same starting character.
        if ((len >= 4) && (utf8_val[0] == '\xc3')) {
            static std::string fe_ff("\xbe\xc3\xbf");
            static std::string ff_fe("\xbf\xc3\xbe");
            static std::string ef_bb_bf("\xaf\xc2\xbb\xc2\xbf");
            // C++-20 has starts_with, but when this was written, qpdf had a minimum supported
            // version of C++-17.
            if ((utf8_val.compare(1, 3, fe_ff) == 0) || (utf8_val.compare(1, 3, ff_fe) == 0) ||
                (utf8_val.compare(1, 5, ef_bb_bf) == 0)) {
                result += unknown;
                okay = false;
            }
        }
        break;
    default:
        break;
    }
    size_t pos = 0;
    while (pos < len) {
        bool error = false;
        unsigned long codepoint = QUtil::get_next_utf8_codepoint(utf8_val, pos, error);
        if (error) {
            okay = false;
            if (encoding == e_utf16) {
                result += "\xff\xfd";
            } else {
                result.append(1, unknown);
            }
        } else if (codepoint < 128) {
            char ch = static_cast<char>(codepoint);
            if (encoding == e_utf16) {
                result += QUtil::toUTF16(QIntC::to_ulong(ch));
            } else if ((encoding == e_pdfdoc) && (((ch >= 0x18) && (ch <= 0x1f)) || (ch == 127))) {
                // PDFDocEncoding maps some low characters to Unicode, so if we encounter those
                // invalid UTF-8 code points, map them to unknown so reversing the mapping doesn't
                // change them into other characters.
                okay = false;
                result.append(1, unknown);
            } else {
                result.append(1, ch);
            }
        } else if (encoding == e_utf16) {
            result += QUtil::toUTF16(codepoint);
        } else if ((codepoint == 0xad) && (encoding == e_pdfdoc)) {
            // PDFDocEncoding omits 0x00ad (soft hyphen).
            okay = false;
            result.append(1, unknown);
        } else if (
            (codepoint > 160) && (codepoint < 256) &&
            ((encoding == e_winansi) || (encoding == e_pdfdoc))) {
            result.append(1, static_cast<char>(codepoint & 0xff));
        } else {
            unsigned char ch = '\0';
            if (encoding == e_winansi) {
                ch = encode_winansi(codepoint);
            } else if (encoding == e_macroman) {
                ch = encode_macroman(codepoint);
            } else if (encoding == e_pdfdoc) {
                ch = encode_pdfdoc(codepoint);
            }
            if (ch == '\0') {
                okay = false;
                ch = static_cast<unsigned char>(unknown);
            }
            result.append(1, static_cast<char>(ch));
        }
    }
    return okay;
}

static std::string
transcode_utf8(std::string const& utf8_val, encoding_e encoding, char unknown)
{
    std::string result;
    transcode_utf8(utf8_val, result, encoding, unknown);
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

std::string
QUtil::utf8_to_pdf_doc(std::string const& utf8, char unknown_char)
{
    return transcode_utf8(utf8, e_pdfdoc, unknown_char);
}

bool
QUtil::utf8_to_ascii(std::string const& utf8, std::string& ascii, char unknown_char)
{
    return transcode_utf8(utf8, ascii, e_ascii, unknown_char);
}

bool
QUtil::utf8_to_win_ansi(std::string const& utf8, std::string& win, char unknown_char)
{
    return transcode_utf8(utf8, win, e_winansi, unknown_char);
}

bool
QUtil::utf8_to_mac_roman(std::string const& utf8, std::string& mac, char unknown_char)
{
    return transcode_utf8(utf8, mac, e_macroman, unknown_char);
}

bool
QUtil::utf8_to_pdf_doc(std::string const& utf8, std::string& pdfdoc, char unknown_char)
{
    return transcode_utf8(utf8, pdfdoc, e_pdfdoc, unknown_char);
}

bool
QUtil::is_utf16(std::string const& val)
{
    return (
        (val.length() >= 2) &&
        (((val.at(0) == '\xfe') && (val.at(1) == '\xff')) ||
         ((val.at(0) == '\xff') && (val.at(1) == '\xfe'))));
}

bool
QUtil::is_explicit_utf8(std::string const& val)
{
    // QPDF_String.cc knows that this is a 3-byte sequence.
    return (
        (val.length() >= 3) && (val.at(0) == '\xef') && (val.at(1) == '\xbb') &&
        (val.at(2) == '\xbf'));
}

std::string
QUtil::utf16_to_utf8(std::string const& val)
{
    std::string result;
    // This code uses unsigned long and unsigned short to hold codepoint values. It requires
    // unsigned long to be at least 32 bits and unsigned short to be at least 16 bits, but it will
    // work fine if they are larger.
    unsigned long codepoint = 0L;
    size_t len = val.length();
    size_t start = 0;
    bool is_le = false;
    if (is_utf16(val)) {
        if (static_cast<unsigned char>(val.at(0)) == 0xff) {
            is_le = true;
        }
        start += 2;
    }
    // If the string has an odd number of bytes, the last byte is ignored.
    for (size_t i = start; i + 1 < len; i += 2) {
        // Convert from UTF16-BE.  If we get a malformed codepoint, this code will generate
        // incorrect output without giving a warning.  Specifically, a high codepoint not followed
        // by a low codepoint will be discarded, and a low codepoint not preceded by a high
        // codepoint will just get its low 10 bits output.
        auto msb = is_le ? i + 1 : i;
        auto lsb = is_le ? i : i + 1;
        unsigned short bits = QIntC::to_ushort(
            (static_cast<unsigned char>(val.at(msb)) << 8) +
            static_cast<unsigned char>(val.at(lsb)));
        if ((bits & 0xFC00) == 0xD800) {
            codepoint = 0x10000U + ((bits & 0x3FFU) << 10U);
            continue;
        } else if ((bits & 0xFC00) == 0xDC00) {
            if (codepoint != 0) {
                QTC::TC("qpdf", "QUtil non-trivial UTF-16");
            }
            codepoint += bits & 0x3FF;
        } else {
            codepoint = bits;
        }

        result += QUtil::toUTF8(codepoint);
        codepoint = 0;
    }
    return result;
}

std::string
QUtil::win_ansi_to_utf8(std::string const& val)
{
    std::string result;
    size_t len = val.length();
    for (unsigned int i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(val.at(i));
        unsigned short ch_short = ch;
        if ((ch >= 128) && (ch <= 160)) {
            ch_short = win_ansi_to_unicode[ch - 128];
        }
        result += QUtil::toUTF8(ch_short);
    }
    return result;
}

std::string
QUtil::mac_roman_to_utf8(std::string const& val)
{
    std::string result;
    size_t len = val.length();
    for (unsigned int i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(val.at(i));
        unsigned short ch_short = ch;
        if (ch >= 128) {
            ch_short = mac_roman_to_unicode[ch - 128];
        }
        result += QUtil::toUTF8(ch_short);
    }
    return result;
}

std::string
QUtil::pdf_doc_to_utf8(std::string const& val)
{
    std::string result;
    size_t len = val.length();
    for (unsigned int i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(val.at(i));
        unsigned short ch_short = ch;
        if ((ch >= 127) && (ch <= 160)) {
            ch_short = pdf_doc_to_unicode[ch - 127];
        } else if ((ch >= 24) && (ch <= 31)) {
            ch_short = pdf_doc_low_to_unicode[ch - 24];
        } else if (ch == 173) {
            ch_short = 0xfffd;
        }
        result += QUtil::toUTF8(ch_short);
    }
    return result;
}

void
QUtil::analyze_encoding(
    std::string const& val, bool& has_8bit_chars, bool& is_valid_utf8, bool& is_utf16)
{
    has_8bit_chars = is_utf16 = is_valid_utf8 = false;
    if (QUtil::is_utf16(val)) {
        has_8bit_chars = true;
        is_utf16 = true;
        return;
    }
    size_t len = val.length();
    size_t pos = 0;
    bool any_errors = false;
    while (pos < len) {
        bool error = false;
        auto o_pos = pos;
        get_next_utf8_codepoint(val, pos, error);
        if (error) {
            any_errors = true;
        }
        if (pos - o_pos > 1 || val[o_pos] & 0x80) {
            has_8bit_chars = true;
        }
    }
    if (has_8bit_chars && (!any_errors)) {
        is_valid_utf8 = true;
    }
}

std::vector<std::string>
QUtil::possible_repaired_encodings(std::string supplied)
{
    std::vector<std::string> result;
    // Always include the original string
    result.push_back(supplied);
    bool has_8bit_chars = false;
    bool is_valid_utf8 = false;
    bool is_utf16 = false;
    analyze_encoding(supplied, has_8bit_chars, is_valid_utf8, is_utf16);
    if (!has_8bit_chars) {
        return result;
    }
    if (is_utf16) {
        // Convert to UTF-8 and pretend we got a UTF-8 string.
        is_utf16 = false;
        is_valid_utf8 = true;
        supplied = utf16_to_utf8(supplied);
    }
    std::string output;
    if (is_valid_utf8) {
        // Maybe we were given UTF-8 but wanted one of the single-byte encodings.
        if (utf8_to_pdf_doc(supplied, output)) {
            result.push_back(output);
        }
        if (utf8_to_win_ansi(supplied, output)) {
            result.push_back(output);
        }
        if (utf8_to_mac_roman(supplied, output)) {
            result.push_back(output);
        }
    } else {
        // Maybe we were given one of the single-byte encodings but wanted UTF-8.
        std::string from_pdf_doc(pdf_doc_to_utf8(supplied));
        result.push_back(from_pdf_doc);
        std::string from_win_ansi(win_ansi_to_utf8(supplied));
        result.push_back(from_win_ansi);
        std::string from_mac_roman(mac_roman_to_utf8(supplied));
        result.push_back(from_mac_roman);

        // Maybe we were given one of the other single-byte encodings but wanted one of the other
        // ones.
        if (utf8_to_win_ansi(from_pdf_doc, output)) {
            result.push_back(output);
        }
        if (utf8_to_mac_roman(from_pdf_doc, output)) {
            result.push_back(output);
        }
        if (utf8_to_pdf_doc(from_win_ansi, output)) {
            result.push_back(output);
        }
        if (utf8_to_mac_roman(from_win_ansi, output)) {
            result.push_back(output);
        }
        if (utf8_to_pdf_doc(from_mac_roman, output)) {
            result.push_back(output);
        }
        if (utf8_to_win_ansi(from_mac_roman, output)) {
            result.push_back(output);
        }
    }
    // De-duplicate
    std::vector<std::string> t;
    std::set<std::string> seen;
    for (auto const& iter: result) {
        if (!seen.count(iter)) {
            seen.insert(iter);
            t.push_back(iter);
        }
    }
    return t;
}

#ifndef QPDF_NO_WCHAR_T
static int
call_main_from_wmain(
    bool, int argc, wchar_t const* const argv[], std::function<int(int, char*[])> realmain)
{
    // argv contains UTF-16-encoded strings with a 16-bit wchar_t. Convert this to UTF-8-encoded
    // strings for compatibility with other systems. That way the rest of qpdf.cc can just act like
    // arguments are UTF-8.

    std::vector<std::unique_ptr<char[]>> utf8_argv;
    for (int i = 0; i < argc; ++i) {
        std::string utf16;
        for (size_t j = 0; j < std::wcslen(argv[i]); ++j) {
            unsigned short codepoint = static_cast<unsigned short>(argv[i][j]);
            utf16.append(1, static_cast<char>(QIntC::to_uchar(codepoint >> 8)));
            utf16.append(1, static_cast<char>(QIntC::to_uchar(codepoint & 0xff)));
        }
        std::string utf8 = QUtil::utf16_to_utf8(utf16);
        utf8_argv.push_back(QUtil::make_unique_cstr(utf8));
    }
    auto utf8_argv_sp = std::make_unique<char*[]>(1 + utf8_argv.size());
    char** new_argv = utf8_argv_sp.get();
    for (size_t i = 0; i < utf8_argv.size(); ++i) {
        new_argv[i] = utf8_argv.at(i).get();
    }
    argc = QIntC::to_int(utf8_argv.size());
    new_argv[argc] = nullptr;
    return realmain(argc, new_argv);
}

int
QUtil::call_main_from_wmain(int argc, wchar_t* argv[], std::function<int(int, char*[])> realmain)
{
    return ::call_main_from_wmain(true, argc, argv, realmain);
}

int
QUtil::call_main_from_wmain(
    int argc, wchar_t const* const argv[], std::function<int(int, char const* const[])> realmain)
{
    return ::call_main_from_wmain(true, argc, argv, [realmain](int new_argc, char* new_argv[]) {
        return realmain(new_argc, new_argv);
    });
}

#endif // QPDF_NO_WCHAR_T

size_t
QUtil::get_max_memory_usage()
{
#if defined(HAVE_MALLOC_INFO) && defined(HAVE_OPEN_MEMSTREAM)
    static std::regex tag_re("<(/?\\w+)([^>]*?)>");
    static std::regex attr_re("(\\w+)=\"(.*?)\"");

    char* buf;
    size_t size;
    FILE* f = open_memstream(&buf, &size);
    if (f == nullptr) {
        return 0;
    }
    malloc_info(0, f);
    fclose(f);
    if (QUtil::get_env("QPDF_DEBUG_MEM_USAGE")) {
        fprintf(stderr, "%s", buf);
    }

    // Warning: this code uses regular expression to extract data from an XML string. This is
    // generally a bad idea, but we're going to do it anyway because QUtil.hh warns against using
    // this function for other than development/testing, and if this function fails to generate
    // reasonable output during performance testing, it will be noticed.

    // This is my best guess at how to interpret malloc_info. Anyway it seems to provide useful
    // information for detecting code changes that drastically change memory usage.
    size_t result = 0;
    try {
        std::cregex_iterator m_begin(buf, buf + size, tag_re);
        std::cregex_iterator cr_end;
        std::sregex_iterator sr_end;

        int in_heap = 0;
        for (auto m = m_begin; m != cr_end; ++m) {
            std::string tag(m->str(1));
            if (tag == "heap") {
                ++in_heap;
            } else if (tag == "/heap") {
                --in_heap;
            } else if (in_heap == 0) {
                std::string rest = m->str(2);
                std::map<std::string, std::string> attrs;
                std::sregex_iterator a_begin(rest.begin(), rest.end(), attr_re);
                for (auto m2 = a_begin; m2 != sr_end; ++m2) {
                    attrs[m2->str(1)] = m2->str(2);
                }
                if (tag == "total") {
                    if (attrs.count("size") > 0) {
                        result += QIntC::to_size(QUtil::string_to_ull(attrs["size"].c_str()));
                    }
                } else if (tag == "system" && attrs["type"] == "max") {
                    result += QIntC::to_size(QUtil::string_to_ull(attrs["size"].c_str()));
                }
            }
        }
    } catch (...) {
        // ignore -- just return 0
    }
    free(buf);
    return result;
#else
    return 0;
#endif
}

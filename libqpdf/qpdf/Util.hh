#ifndef UTIL_HH
#define UTIL_HH

#include <qpdf/assert_debug.h>

#include <qpdf/qpdf-config.h>

#include <charconv>
#include <clocale>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std::literals;

namespace qpdf::util
{
    // qpdf::util is a collection of useful utility functions for qpdf internal use. It includes
    // inline functions, some of which are exposed as regular functions in QUtil. Implementations
    // are in QUtil.cc.

    // Throw a logic_error if 'cond' does not hold.
    //
    // DO NOT USE unless it is impractical or unnecessary to cover violations during CI Testing.
    template <typename T>
    inline void
    assertion(bool cond, T&& msg)
    {
        if (!cond) {
            throw std::logic_error(std::forward<T>(msg));
        }
    }

    template <typename T>
    inline void
    internal_error_if(bool cond, T&& msg)
    {
        if (cond) {
            throw std::logic_error("INTERNAL ERROR: "s.append(std::forward<T>(msg))
                                       .append(
                                           "\nThis is a qpdf bug. Please report at "
                                           "https://github.com/qpdf/qpdf/issues"));
        }
    }

    template <typename T>
    inline void
    no_ci_rt_error_if(bool cond, T&& msg)
    {
        if (cond) {
            throw std::runtime_error(std::forward<T>(msg));
        }
    }

    inline constexpr char
    hex_decode_char(char digit)
    {
        return digit <= '9' && digit >= '0'
            ? char(digit - '0')
            : (digit >= 'a' ? char(digit - 'a' + 10)
                            : (digit >= 'A' ? char(digit - 'A' + 10) : '\20'));
    }

    inline constexpr bool
    is_hex_digit(char ch)
    {
        return hex_decode_char(ch) < '\20';
    }

    inline constexpr bool
    is_space(char ch)
    {
        return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\f' || ch == '\v';
    }

    inline bool
    is_digit(char ch)
    {
        return (ch >= '0' && ch <= '9');
    }

    // Returns lower-case hex-encoded version of the char including a leading "#".
    inline std::string
    hex_encode_char(char c)
    {
        static auto constexpr hexchars = "0123456789abcdef";
        return {'#', hexchars[static_cast<unsigned char>(c) >> 4], hexchars[c & 0x0f]};
    }

    // Numerically increment a digit string. Ignore the last 'tail' characters.
    inline void
    increment(std::string& s, int tail = 0)
    {
        auto end = s.rend();
        for (auto it = s.rbegin() + tail; it != end; ++it) {
            ++*it;
            if (*it != ':') {
                return;
            }
            *it = '0';
        }
        s.insert(0, 1, '1');
    }

    inline bool
    is_utf16(std::string const& str)
    {
        return str.starts_with("\xfe\xff") || str.starts_with("\xff\xfe");
    }

    inline bool
    is_explicit_utf8(std::string const& str)
    {
        // QPDF_String.cc knows that this is a 3-byte sequence.
        return str.starts_with("\xef\xbb\xbf");
    }

    std::string random_string(size_t len);

    /// @brief Convert a PDF numeric token to double.
    ///
    /// Uses `std::from_chars` for strict parsing when floating-point overloads are available.
    /// In fallback builds, uses `std::atof` and normalizes `.` to the current locale decimal point
    /// before conversion.
    ///
    /// @param val input numeric token
    /// @return `{true, value}` if conversion succeeds (`from_chars` path) or always in fallback
    /// mode; `{false, value}` on strict-parse failure
    inline std::pair<bool, double>
    str_to_double(std::string const& val)
    {
#ifdef HAVE_FROM_CHARS_DOUBLE
        size_t skip = val.starts_with('+') ? 1 : 0;
        if (val.size() <= skip) {
            return {false, 0};
        }
        double result = 0.0;
        auto end = val.data() + val.size();
        auto [tail, ec] = std::from_chars(val.data() + skip, end, result, std::chars_format::fixed);
        if (ec == std::errc() && tail == end) {
            return {true, result};
        }
        (void)std::from_chars(val.data() + skip, end, result, std::chars_format::general);
        return {false, result};
#else
        // Use atof if floating point std::from_chars is not available
        auto locale_decimal_point = []() {
            if (auto* lconv = std::localeconv();
                lconv && lconv->decimal_point && lconv->decimal_point[0] != '\0') {
                return lconv->decimal_point[0];
            }
            return '.';
        };
        static char const decimal_point = locale_decimal_point();
        if (decimal_point == '.') {
            return {true, std::atof(val.data())};
        }
        auto copy_str = val;
        for (char& ch: copy_str) {
            if (ch == '.') {
                ch = decimal_point;
                break;
            }
        }
        return {true, std::atof(copy_str.data())};
#endif
    }

} // namespace qpdf::util

#endif // UTIL_HH

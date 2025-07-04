#ifndef UTIL_HH
#define UTIL_HH

#include <string>

namespace qpdf::util
{
    // This is a collection of useful utility functions for qpdf internal use. They include inline
    // functions, some of which are exposed as regular functions in QUtil. Implementations are in
    // QUtil.cc.

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

    std::string random_string(size_t len);

} // namespace qpdf::util

#endif // UTIL_HH

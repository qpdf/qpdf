#ifndef UTIL_HH
#define UTIL_HH

#include <qpdf/assert_debug.h>

#include <concepts>
#include <cstdint>
#include <limits>
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

    /// @brief Return true if `val` fits in `T` (predicate only).
    ///
    /// @tparam T integral target type
    /// @param val value to test
    /// @return true if `val` fits in `T`
    template <typename T>
        requires std::integral<T>
    bool
    fits(std::integral auto val)
    {
        if constexpr (std::cmp_less(
                          std::numeric_limits<decltype(val)>::min(),
                          std::numeric_limits<T>::min())) {
            if (std::cmp_less(val, std::numeric_limits<T>::min())) {
                return false;
            }
        }
        if constexpr (std::cmp_greater(
                          std::numeric_limits<decltype(val)>::max(),
                          std::numeric_limits<T>::max())) {
            if (std::cmp_greater(val, std::numeric_limits<T>::max())) {
                return false;
            }
        }
        return true;
    }

    /// @brief Convert `val` to `T`; throws std::range_error if out-of-range.
    ///
    /// @tparam T integral target type
    /// @param val value to convert
    /// @return Converted value as `T`
    template <typename T>
        requires std::integral<T>
    T
    to(std::integral auto val)
    {
        if (!fits<T>(val)) {
            throw std::range_error("out of range converting integral values");
        }
        return static_cast<T>(val);
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

    // Test helpers

    /// @brief Predicate: returns true if invoking `f()` throws an exception of type `E`.
    ///
    /// Internal test helper used by unit tests: call `throws<SomeException>([](){ ... })` and
    /// it returns true when the callable throws `SomeException` and false otherwise.
    /// The callable must be invocable with no arguments.
    template <typename E, typename F>
        requires std::invocable<F>
    inline bool
    throws(F&& f)
    {
        try {
            std::forward<F>(f)();
            return false;
        } catch (E const&) {
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace qpdf::util

#endif // UTIL_HH

#include <qpdf/assert_test.h>

#include <qpdf/QIntC.hh>
#include <cstdint>

#define try_convert(exp_pass, fn, i) try_convert_real(#fn "(" #i ")", exp_pass, fn, i)

template <typename From, typename To>
static void
try_convert_real(char const* description, bool exp_pass, To (*fn)(From const&), From const& i)
{
    bool passed = false;
    try {
        To result = fn(i);
        passed = true;
        std::cout << description << ": " << i << " " << result;
    } catch (std::range_error& e) {
        std::cout << description << ": " << e.what();
        passed = false;
    }
    std::cout << ((passed == exp_pass) ? " PASSED" : " FAILED") << std::endl;
}

#define try_range_check(exp_pass, a, b) try_range_check_real(#a " + " #b, exp_pass, a, b)

template <typename T>
static void
try_range_check_real(char const* description, bool exp_pass, T const& a, T const& b)
{
    bool passed = false;
    try {
        QIntC::range_check(a, b);
        std::cout << description << ": okay";
        passed = true;
    } catch (std::range_error& e) {
        std::cout << description << ": " << e.what();
        passed = false;
    }
    std::cout << ((passed == exp_pass) ? " PASSED" : " FAILED") << std::endl;
}

#define try_range_check_subtract(exp_pass, a, b) \
    try_range_check_subtract_real(#a " - " #b, exp_pass, a, b)

template <typename T>
static void
try_range_check_subtract_real(char const* description, bool exp_pass, T const& a, T const& b)
{
    bool passed = false;
    try {
        QIntC::range_check_substract(a, b);
        std::cout << description << ": okay";
        passed = true;
    } catch (std::range_error& e) {
        std::cout << description << ": " << e.what();
        passed = false;
    }
    std::cout << ((passed == exp_pass) ? " PASSED" : " FAILED") << std::endl;
}

int
main()
{
    uint32_t u1 = 3141592653U;                  // Too big for signed type
    int32_t i1 = -1153374643;                   // Same bit pattern as u1
    uint64_t ul1 = 1099511627776LL;             // Too big for 32-bit
    uint64_t ul2 = 12345;                       // Fits into 32-bit
    int32_t i2 = 81;                            // Fits in char and uchar
    auto c1 = static_cast<signed char>('\xf7'); // Signed value when char
    char c2 = 'W';                              // char; may be signed or unsigned

    // Verify i1 and u1 have same bit pattern
    assert(static_cast<uint32_t>(i1) == u1);
    // Verify that we can unsafely convert between signed and unsigned char
    assert(c1 == static_cast<signed char>(static_cast<unsigned char>(c1)));

    try_convert(true, QIntC::to_int<int32_t>, i1);
    try_convert(true, QIntC::to_uint<uint32_t>, u1);
    try_convert(false, QIntC::to_int<uint32_t>, u1);
    try_convert(false, QIntC::to_uint<int32_t>, i1);
    try_convert(false, QIntC::to_int<uint64_t>, ul1);
    try_convert(true, QIntC::to_int<uint64_t>, ul2);
    try_convert(true, QIntC::to_uint<uint64_t>, ul2);
    try_convert(true, QIntC::to_offset<uint32_t>, u1);
    try_convert(true, QIntC::to_offset<int32_t>, i1);
    try_convert(false, QIntC::to_ulonglong<int32_t>, i1);
    try_convert(true, QIntC::to_char<int32_t>, i2);
    try_convert(true, QIntC::to_uchar<int32_t>, i2);
    try_convert(false, QIntC::to_uchar<signed char>, c1);
    try_convert(true, QIntC::to_uchar<char>, c2);
    try_convert(true, QIntC::to_char<char>, c2);

    auto constexpr max_ll = std::numeric_limits<long long>::max();
    auto constexpr max_ull = std::numeric_limits<unsigned long long>::max();
    auto constexpr min_ll = std::numeric_limits<long long>::min();
    auto constexpr max_sc = std::numeric_limits<signed char>::max();
    try_range_check(true, 1, 2);
    try_range_check(true, -1, 2);
    try_range_check(true, -100, -200);
    try_range_check(true, max_ll, 0LL);
    try_range_check(false, max_ll, 1LL);
    try_range_check(true, max_ll, 0LL);
    try_range_check(false, max_ll, 1LL);
    try_range_check(true, max_ull, 0ULL);
    try_range_check(false, max_ull, 1ULL);
    try_range_check(true, min_ll, 0LL);
    try_range_check(false, min_ll, -1LL);
    try_range_check(false, max_sc, max_sc);
    try_range_check(true, '!', '#');
    try_range_check_subtract(true, 1, 2);
    try_range_check_subtract(true, -1, -2);
    try_range_check_subtract(true, 1, 10);
    try_range_check_subtract(true, -1, -10);
    try_range_check_subtract(false, 0LL, min_ll);
    try_range_check_subtract(false, 1LL, min_ll);
    try_range_check_subtract(true, 0LL, max_ll);
    try_range_check_subtract(true, -1LL, max_ll);
    try_range_check_subtract(false, -2LL, max_ll);
    return 0;
}

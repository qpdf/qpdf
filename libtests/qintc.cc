#include <qpdf/QIntC.hh>
#include <stdint.h>
#include <cassert>

#define try_convert(exp_pass, fn, i) \
    try_convert_real(#fn "(" #i ")", exp_pass, fn, i)

template <typename From, typename To>
static void try_convert_real(
    char const* description, bool exp_pass,
    To (*fn)(From const&), From const& i)
{
    bool passed = false;
    try
    {
        To result = fn(i);
        passed = true;
        std::cout << description << ": " << i << " " << result;
    }
    catch (std::range_error& e)
    {
        std::cout << description << ": " << e.what();
        passed = false;
    }
    std::cout << ((passed == exp_pass) ? " PASSED" : " FAILED") << std::endl;
}

int main()
{
    uint32_t u1 = 3141592653U;      // Too big for signed type
    int32_t i1 = -1153374643;       // Same bit pattern as u1
    uint64_t ul1 = 1099511627776LL; // Too big for 32-bit
    uint64_t ul2 = 12345;           // Fits into 32-bit
    int32_t i2 = 81;                // Fits in char and uchar
    char c1 = '\xf7';               // Signed value when char

    // Verify i1 and u1 have same bit pattern
    assert(static_cast<uint32_t>(i1) == u1);
    // Verify that we can unsafely convert between char and unsigned char
    assert(c1 == static_cast<char>(static_cast<unsigned char>(c1)));

    try_convert(true,  QIntC::to_int<int32_t>, i1);
    try_convert(true,  QIntC::to_uint<uint32_t>, u1);
    try_convert(false, QIntC::to_int<uint32_t>, u1);
    try_convert(false, QIntC::to_uint<int32_t>, i1);
    try_convert(false, QIntC::to_int<uint64_t>, ul1);
    try_convert(true,  QIntC::to_int<uint64_t>, ul2);
    try_convert(true,  QIntC::to_uint<uint64_t>, ul2);
    try_convert(true,  QIntC::to_offset<uint32_t>, u1);
    try_convert(true,  QIntC::to_offset<int32_t>, i1);
    try_convert(false, QIntC::to_ulonglong<int32_t>, i1);
    try_convert(true,  QIntC::to_char<int32_t>, i2);
    try_convert(true,  QIntC::to_uchar<int32_t>, i2);
    try_convert(false, QIntC::to_uchar<char>, c1);

    return 0;
}

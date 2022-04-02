#ifndef __BITS_FUNCTIONS_HH__
#define __BITS_FUNCTIONS_HH__

#include <qpdf/Pipeline.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <algorithm>
#include <stdexcept>

// This file is #included by specific source files, which must define
// certain preprocessor symbols. These functions may be run at places
// where the function call overhead from test coverage testing would
// be too high. Therefore, we make the test coverage cases conditional
// upon a preprocessor symbol. Library code includes this file without
// BITS_TESTING, and the specially designed test code that fully
// exercises this code includes with the symbol defined.

#ifdef BITS_READ
static unsigned long long
read_bits(
    unsigned char const*& p,
    size_t& bit_offset,
    size_t& bits_available,
    size_t bits_wanted)
{
    // View p as a stream of bits:

    // 76543210 76543210 ....

    // bit_offset is the bit number within the first byte that marks
    // the first bit that we would read.

    if (bits_wanted > bits_available) {
        throw std::runtime_error(
            "overflow reading bit stream: wanted = " +
            QUtil::uint_to_string(bits_wanted) +
            "; available = " + QUtil::uint_to_string(bits_available));
    }
    if (bits_wanted > 32) {
        throw std::out_of_range("read_bits: too many bits requested");
    }

    unsigned long result = 0;
# ifdef BITS_TESTING
    if (bits_wanted == 0) {
        QTC::TC("libtests", "bits zero bits wanted");
    }
# endif
    while (bits_wanted > 0) {
        // Grab bits from the first byte clearing anything before
        // bit_offset.
        unsigned char byte =
            static_cast<unsigned char>(*p & ((1U << (bit_offset + 1U)) - 1U));

        // There are bit_offset + 1 bits available in the first byte.
        size_t to_copy = std::min(bits_wanted, bit_offset + 1);
        size_t leftover = (bit_offset + 1) - to_copy;

# ifdef BITS_TESTING
        QTC::TC(
            "libtests",
            "bits bit_offset",
            ((bit_offset == 0)       ? 0
                 : (bit_offset == 7) ? 1
                                     : 2));
        QTC::TC("libtests", "bits leftover", (leftover > 0) ? 1 : 0);
# endif

        // Right shift so that all the bits we want are right justified.
        byte = static_cast<unsigned char>(byte >> leftover);

        // Copy the bits into result
        result <<= to_copy;
        result |= byte;

        // Update pointers
        if (leftover) {
            bit_offset = leftover - 1;
        } else {
            bit_offset = 7;
            ++p;
        }
        bits_wanted -= to_copy;
        bits_available -= to_copy;

# ifdef BITS_TESTING
        QTC::TC(
            "libtests",
            "bits iterations",
            ((bits_wanted > 8)       ? 0
                 : (bits_wanted > 0) ? 1
                                     : 2));
# endif
    }

    return result;
}
#endif

#ifdef BITS_WRITE
static void
write_bits(
    unsigned char& ch,
    size_t& bit_offset,
    unsigned long long val,
    size_t bits,
    Pipeline* pipeline)
{
    if (bits > 32) {
        throw std::out_of_range("write_bits: too many bits requested");
    }

    // bit_offset + 1 is the number of bits left in ch
# ifdef BITS_TESTING
    if (bits == 0) {
        QTC::TC("libtests", "bits write zero bits");
    }
# endif
    while (bits > 0) {
        size_t bits_to_write = std::min(bits, bit_offset + 1);
        unsigned char newval = static_cast<unsigned char>(
            (val >> (bits - bits_to_write)) & ((1U << bits_to_write) - 1));
        size_t bits_left_in_ch = bit_offset + 1 - bits_to_write;
        newval = static_cast<unsigned char>(newval << bits_left_in_ch);
        ch |= newval;
        if (bits_left_in_ch == 0) {
# ifdef BITS_TESTING
            QTC::TC("libtests", "bits write pipeline");
# endif
            pipeline->write(&ch, 1);
            bit_offset = 7;
            ch = 0;
        } else {
# ifdef BITS_TESTING
            QTC::TC("libtests", "bits write leftover");
# endif
            bit_offset -= bits_to_write;
        }
        bits -= bits_to_write;
# ifdef BITS_TESTING
        QTC::TC(
            "libtests",
            "bits write iterations",
            ((bits > 8)       ? 0
                 : (bits > 0) ? 1
                              : 2));
# endif
    }
}
#endif

#endif // __BITS_FUNCTIONS_HH__

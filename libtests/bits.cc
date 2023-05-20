#include <qpdf/BitStream.hh>
#include <qpdf/BitWriter.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdlib.h>

// See comments in bits_functions.hh
#define BITS_TESTING 1
#define BITS_READ 1
#define BITS_WRITE 1
#include <qpdf/bits_functions.hh>

static void
print_values(long long byte_offset, size_t bit_offset, size_t bits_available)
{
    std::cout << "byte offset = " << byte_offset << ", "
              << "bit offset = " << bit_offset << ", "
              << "bits available = " << bits_available << std::endl;
}

static void
test_read_bits(
    unsigned char const* buf,
    unsigned char const*& p,
    size_t& bit_offset,
    size_t& bits_available,
    size_t bits_wanted)
{
    unsigned long result =
        QIntC::to_ulong(read_bits(p, bit_offset, bits_available, bits_wanted));

    std::cout << "bits read: " << bits_wanted << ", result = " << result
              << std::endl;
    print_values(p - buf, bit_offset, bits_available);
}

static void
test_write_bits(
    unsigned char& ch,
    size_t& bit_offset,
    unsigned long val,
    size_t bits,
    Pl_Buffer* bp)
{
    write_bits(ch, bit_offset, val, bits, bp);
    std::cout << "ch = " << QUtil::uint_to_string_base(ch, 16, 2)
              << ", bit_offset = " << bit_offset << std::endl;
}

static void
print_buffer(Pl_Buffer* bp)
{
    bp->finish();
    Buffer* b = bp->getBuffer();
    unsigned char const* p = b->getBuffer();
    size_t l = b->getSize();
    for (unsigned long i = 0; i < l; ++i) {
        std::cout << QUtil::uint_to_string_base(p[i], 16, 2)
                  << ((i == l - 1) ? "\n" : " ");
    }
    std::cout << std::endl;
    delete b;
}

static void
test()
{
    // 11110101 00010101 01100101 01111001 00010010 10001001 01110101 01001011
    // F5 15 65 79 12 89 75 4B

    // Read tests

    static unsigned char const buf[] = {
        0xF5, 0x15, 0x65, 0x79, 0x12, 0x89, 0x75, 0x4B};

    unsigned char const* p = buf;
    size_t bit_offset = 7;
    size_t bits_available = 64;

    // 11110:101 0:001010:1 01100101: 01111001
    // 0:00:1:0010 10001001 01110101 01001:011
    print_values(p - buf, bit_offset, bits_available);
    test_read_bits(buf, p, bit_offset, bits_available, 5);
    test_read_bits(buf, p, bit_offset, bits_available, 4);
    test_read_bits(buf, p, bit_offset, bits_available, 6);
    test_read_bits(buf, p, bit_offset, bits_available, 9);
    test_read_bits(buf, p, bit_offset, bits_available, 9);
    test_read_bits(buf, p, bit_offset, bits_available, 2);
    test_read_bits(buf, p, bit_offset, bits_available, 1);
    test_read_bits(buf, p, bit_offset, bits_available, 0);
    test_read_bits(buf, p, bit_offset, bits_available, 25);

    try {
        test_read_bits(buf, p, bit_offset, bits_available, 4);
    } catch (std::exception& e) {
        std::cout << "exception: " << e.what() << std::endl;
        print_values(p - buf, bit_offset, bits_available);
    }

    test_read_bits(buf, p, bit_offset, bits_available, 3);
    std::cout << std::endl;

    // 11110101 00010101 01100101 01111001: 00010010 10001001 01110101 01001011

    p = buf;
    bit_offset = 7;
    bits_available = 64;
    print_values(p - buf, bit_offset, bits_available);
    test_read_bits(buf, p, bit_offset, bits_available, 32);
    test_read_bits(buf, p, bit_offset, bits_available, 32);
    std::cout << std::endl;

    BitStream b(buf, 8);
    std::cout << b.getBits(32) << std::endl;
    b.reset();
    std::cout << b.getBits(32) << std::endl;
    std::cout << b.getBits(32) << std::endl;
    std::cout << std::endl;

    b.reset();
    std::cout << b.getBits(6) << std::endl;
    b.skipToNextByte();
    std::cout << b.getBits(8) << std::endl;
    b.skipToNextByte();
    std::cout << b.getBits(8) << std::endl;
    std::cout << std::endl;
    b.reset();
    std::cout << b.getBitsSigned(3) << std::endl;
    std::cout << b.getBitsSigned(6) << std::endl;
    std::cout << b.getBitsSigned(5) << std::endl;
    std::cout << b.getBitsSigned(1) << std::endl;
    std::cout << b.getBitsSigned(17) << std::endl;
    std::cout << std::endl;

    // Write tests

    // 11110:101 0:001010:1 01100101: 01111001
    // 0:00:1:0010 10001001 01110101 01001:011

    unsigned char ch = 0;
    bit_offset = 7;
    Pl_Buffer* bp = new Pl_Buffer("buffer");

    test_write_bits(ch, bit_offset, 30UL, 5, bp);
    test_write_bits(ch, bit_offset, 10UL, 4, bp);
    test_write_bits(ch, bit_offset, 10UL, 6, bp);
    test_write_bits(ch, bit_offset, 16059UL, 0, bp);
    test_write_bits(ch, bit_offset, 357UL, 9, bp);
    print_buffer(bp);

    test_write_bits(ch, bit_offset, 242UL, 9, bp);
    test_write_bits(ch, bit_offset, 0UL, 2, bp);
    test_write_bits(ch, bit_offset, 1UL, 1, bp);
    test_write_bits(ch, bit_offset, 5320361UL, 25, bp);
    test_write_bits(ch, bit_offset, 3UL, 3, bp);

    print_buffer(bp);
    test_write_bits(ch, bit_offset, 4111820153UL, 32, bp);
    test_write_bits(ch, bit_offset, 310998347UL, 32, bp);
    print_buffer(bp);

    BitWriter bw(bp);
    bw.writeBits(30UL, 5);
    bw.flush();
    bw.flush();
    bw.writeBitsInt(0xAB, 8);
    bw.flush();
    print_buffer(bp);
    bw.writeBitsSigned(-1, 3);  // 111
    bw.writeBitsSigned(-12, 6); // 110100
    bw.writeBitsSigned(4, 3);   // 100
    bw.writeBitsSigned(-4, 3);  // 100
    bw.writeBitsSigned(-1, 1);  // 1
    bw.flush();
    print_buffer(bp);

    delete bp;
}

int
main()
{
    try {
        test();
    } catch (std::exception& e) {
        std::cout << "unexpected exception: " << e.what() << std::endl;
        exit(2);
    }
    std::cout << "done" << std::endl;
    return 0;
}

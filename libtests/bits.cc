#include <qpdf/BitStream.hh>
#include <qpdf/BitWriter.hh>
#include <qpdf/Pl_Buffer.hh>
#include <iostream>
#include <stdio.h>

// See comments in bits.cc
#define BITS_TESTING 1
#define BITS_READ 1
#define BITS_WRITE 1
#include "../libqpdf/bits.icc"

static void
print_values(int byte_offset, unsigned int bit_offset,
	     unsigned int bits_available)
{
    std::cout << "byte offset = " << byte_offset << ", "
	      << "bit offset = " << bit_offset << ", "
	      << "bits available = " << bits_available << std::endl;
}

static void
test_read_bits(unsigned char const* buf,
	       unsigned char const*& p, unsigned int& bit_offset,
	       unsigned int& bits_available, int bits_wanted)
{
    unsigned long result =
	read_bits(p, bit_offset, bits_available, bits_wanted);

    std::cout << "bits read: " << bits_wanted << ", result = " << result
	      << std::endl;
    print_values(p - buf, bit_offset, bits_available);
}

static void
test_write_bits(unsigned char& ch, unsigned int& bit_offset, unsigned long val,
		int bits, Pl_Buffer* bp)
{
    write_bits(ch, bit_offset, val, bits, bp);
    printf("ch = %02x, bit_offset = %d\n",
           static_cast<unsigned int>(ch), bit_offset);
}

static void
print_buffer(Pl_Buffer* bp)
{
    bp->finish();
    Buffer* b = bp->getBuffer();
    unsigned char const* p = b->getBuffer();
    size_t l = b->getSize();
    for (unsigned long i = 0; i < l; ++i)
    {
	printf("%02x%s", static_cast<unsigned int>(p[i]),
	       (i == l - 1) ? "\n" : " ");
    }
    printf("\n");
    delete b;
}

static void
test()
{
    // 11110101 00010101 01100101 01111001 00010010 10001001 01110101 01001011
    // F5 15 65 79 12 89 75 4B

    // Read tests

    static unsigned char const buf[] = {
	0xF5, 0x15, 0x65, 0x79, 0x12, 0x89, 0x75, 0x4B
    };

    unsigned char const* p = buf;
    unsigned int bit_offset = 7;
    unsigned int bits_available = 64;

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

    try
    {
	test_read_bits(buf, p, bit_offset, bits_available, 4);
    }
    catch (std::exception& e)
    {
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
    bw.writeBits(0xABUL, 8);
    bw.flush();
    print_buffer(bp);

    delete bp;
}

int main()
{
    try
    {
	test();
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
	exit(2);
    }
    std::cout << "done" << std::endl;
    return 0;
}

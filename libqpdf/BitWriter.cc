#include <qpdf/BitWriter.hh>

// See comments in bits.cc
#define BITS_WRITE 1
#include "bits.icc"

BitWriter::BitWriter(Pipeline* pl) :
    pl(pl),
    ch(0),
    bit_offset(7)
{
}

void
BitWriter::writeBits(unsigned long long val, unsigned int bits)
{
    write_bits(this->ch, this->bit_offset, val, bits, this->pl);
}

void
BitWriter::flush()
{
    if (bit_offset < 7)
    {
	int bits_to_write = bit_offset + 1;
	write_bits(this->ch, this->bit_offset, 0, bits_to_write, this->pl);
    }
}

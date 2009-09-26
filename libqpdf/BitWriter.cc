

#include <qpdf/BitWriter.hh>

// See comments in bits.cc
#define BITS_WRITE 1
#include "bits.icc"

DLL_EXPORT
BitWriter::BitWriter(Pipeline* pl) :
    pl(pl),
    ch(0),
    bit_offset(7)
{
}

DLL_EXPORT
void
BitWriter::writeBits(unsigned long val, int bits)
{
    write_bits(this->ch, this->bit_offset, val, bits, this->pl);
}

DLL_EXPORT
void
BitWriter::flush()
{
    if (bit_offset < 7)
    {
	int bits_to_write = bit_offset + 1;
	write_bits(this->ch, this->bit_offset, 0, bits_to_write, this->pl);
    }
}

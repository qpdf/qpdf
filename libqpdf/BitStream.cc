#include <qpdf/BitStream.hh>

// See comments in bits.cc
#define BITS_READ 1
#include "bits.icc"

BitStream::BitStream(unsigned char const* p, int nbytes) :
    start(p),
    nbytes(nbytes)
{
    reset();
}

void
BitStream::reset()
{
    p = start;
    bit_offset = 7;
    if (static_cast<unsigned int>(nbytes) > static_cast<unsigned int>(-1) / 8)
    {
	throw std::runtime_error("array too large for bitstream");
    }
    bits_available = 8 * nbytes;
}

unsigned long long
BitStream::getBits(int nbits)
{
    return read_bits(this->p, this->bit_offset,
		     this->bits_available, nbits);
}

void
BitStream::skipToNextByte()
{
    if (bit_offset != 7)
    {
	unsigned int bits_to_skip = bit_offset + 1;
	if (bits_available < bits_to_skip)
	{
	    throw std::logic_error(
		"INTERNAL ERROR: overflow skipping to next byte in bitstream");
	}
	bit_offset = 7;
	++p;
	bits_available -= bits_to_skip;
    }
}

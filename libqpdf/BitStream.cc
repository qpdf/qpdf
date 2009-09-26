#include <qpdf/BitStream.hh>

// See comments in bits.cc
#define BITS_READ 1
#include "bits.icc"

DLL_EXPORT
BitStream::BitStream(unsigned char const* p, int nbytes) :
    start(p),
    nbytes(nbytes)
{
    reset();
}

DLL_EXPORT
void
BitStream::reset()
{
    p = start;
    bit_offset = 7;
    bits_available = 8 * nbytes;
}

DLL_EXPORT
unsigned long
BitStream::getBits(int nbits)
{
    return read_bits(this->p, this->bit_offset,
		     this->bits_available, nbits);
}

DLL_EXPORT
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

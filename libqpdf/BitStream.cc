#include <qpdf/BitStream.hh>

#include <qpdf/QIntC.hh>

// See comments in bits.cc
#define BITS_READ 1
#include "bits.icc"

BitStream::BitStream(unsigned char const* p, size_t nbytes) :
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
    if (QIntC::to_uint(nbytes) > static_cast<unsigned int>(-1) / 8)
    {
	throw std::runtime_error("array too large for bitstream");
    }
    bits_available = 8 * nbytes;
}

unsigned long long
BitStream::getBits(size_t nbits)
{
    return read_bits(this->p, this->bit_offset,
		     this->bits_available, nbits);
}

long long
BitStream::getBitsSigned(size_t nbits)
{
    unsigned long long bits = read_bits(this->p, this->bit_offset,
                                        this->bits_available, nbits);
    long long result = 0;
    if (static_cast<long long>(bits) > 1LL << (nbits - 1))
    {
        result = static_cast<long long>(bits -(1ULL << nbits));
    }
    else
    {
        result = static_cast<long long>(bits);
    }
    return result;
}

int
BitStream::getBitsInt(size_t nbits)
{
    return static_cast<int>(
        QIntC::to_uint(
            read_bits(this->p, this->bit_offset,
                      this->bits_available, nbits)));
}

void
BitStream::skipToNextByte()
{
    if (bit_offset != 7)
    {
	size_t bits_to_skip = bit_offset + 1;
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

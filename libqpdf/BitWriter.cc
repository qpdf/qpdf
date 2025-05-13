#include <qpdf/BitWriter.hh>

// See comments in bits_functions.hh
#define BITS_WRITE 1
#include <qpdf/bits_functions.hh>

BitWriter::BitWriter(Pipeline* pl) :
    pl(pl)
{
}

void
BitWriter::writeBits(unsigned long long val, size_t bits)
{
    write_bits(this->ch, this->bit_offset, val, bits, this->pl);
}

void
BitWriter::writeBitsSigned(long long val, size_t bits)
{
    unsigned long long uval = 0;
    if (val < 0) {
        uval = (1ULL << bits) + static_cast<unsigned long long>(val);
    } else {
        uval = static_cast<unsigned long long>(val);
    }
    writeBits(uval, bits);
}

void
BitWriter::writeBitsInt(int val, size_t bits)
{
    writeBits(static_cast<unsigned long long>(val), bits);
}

void
BitWriter::flush()
{
    if (bit_offset < 7) {
        size_t bits_to_write = bit_offset + 1;
        write_bits(this->ch, this->bit_offset, 0, bits_to_write, this->pl);
    }
}

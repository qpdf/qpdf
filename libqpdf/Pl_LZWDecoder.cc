#include <qpdf/Pl_LZWDecoder.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <stdexcept>
#include <string.h>
#include <assert.h>

Pl_LZWDecoder::Pl_LZWDecoder(char const* identifier, Pipeline* next,
			     bool early_code_change) :
    Pipeline(identifier, next),
    code_size(9),
    next(0),
    byte_pos(0),
    bit_pos(0),
    bits_available(0),
    code_change_delta(early_code_change ? 1 : 0),
    eod(false),
    last_code(256)
{
    memset(buf, 0, 3);
}

Pl_LZWDecoder::~Pl_LZWDecoder()
{
}

void
Pl_LZWDecoder::write(unsigned char* bytes, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
	this->buf[next++] = bytes[i];
	if (this->next == 3)
	{
	    this->next = 0;
	}
	this->bits_available += 8;
	if (this->bits_available >= this->code_size)
	{
	    sendNextCode();
	}
    }
}

void
Pl_LZWDecoder::finish()
{
    getNext()->finish();
}

void
Pl_LZWDecoder::sendNextCode()
{
    int high = this->byte_pos;
    int med = (this->byte_pos + 1) % 3;
    int low = (this->byte_pos + 2) % 3;

    int bits_from_high = 8 - this->bit_pos;
    int bits_from_med = this->code_size - bits_from_high;
    int bits_from_low = 0;
    if (bits_from_med > 8)
    {
	bits_from_low = bits_from_med - 8;
	bits_from_med = 8;
    }
    int high_mask = (1 << bits_from_high) - 1;
    int med_mask = 0xff - ((1 << (8 - bits_from_med)) - 1);
    int low_mask = 0xff - ((1 << (8 - bits_from_low)) - 1);
    int code = 0;
    code += (this->buf[high] & high_mask) << bits_from_med;
    code += ((this->buf[med] & med_mask) >> (8 - bits_from_med));
    if (bits_from_low)
    {
	code <<= bits_from_low;
	code += ((this->buf[low] & low_mask) >> (8 - bits_from_low));
	this->byte_pos = low;
	this->bit_pos = bits_from_low;
    }
    else
    {
	this->byte_pos = med;
	this->bit_pos = bits_from_med;
    }
    if (this->bit_pos == 8)
    {
	this->bit_pos = 0;
	++this->byte_pos;
	this->byte_pos %= 3;
    }
    this->bits_available -= this->code_size;

    handleCode(code);
}

unsigned char
Pl_LZWDecoder::getFirstChar(int code)
{
    unsigned char result = '\0';
    if (code < 256)
    {
	result = static_cast<unsigned char>(code);
    }
    else if (code > 257)
    {
	unsigned int idx = code - 258;
	if (idx >= table.size())
        {
            throw std::logic_error(
                "Pl_LZWDecoder::getFirstChar: table overflow");
        }
	Buffer& b = table.at(idx);
	result = b.getBuffer()[0];
    }
    else
    {
        throw std::logic_error(
            "Pl_LZWDecoder::getFirstChar called with invalid code (" +
            QUtil::int_to_string(code) + ")");
    }
    return result;
}

void
Pl_LZWDecoder::addToTable(unsigned char next)
{
    unsigned int last_size = 0;
    unsigned char const* last_data = 0;
    unsigned char tmp[1];

    if (this->last_code < 256)
    {
	tmp[0] = this->last_code;
	last_data = tmp;
	last_size = 1;
    }
    else if (this->last_code > 257)
    {
	unsigned int idx = this->last_code - 258;
	if (idx >= table.size())
        {
            throw std::logic_error(
                "Pl_LZWDecoder::addToTable: table overflow");
        }
	Buffer& b = table.at(idx);
	last_data = b.getBuffer();
	last_size = b.getSize();
    }
    else
    {
        throw std::logic_error(
            "Pl_LZWDecoder::addToTable called with invalid code (" +
            QUtil::int_to_string(this->last_code) + ")");
    }

    Buffer entry(1 + last_size);
    unsigned char* new_data = entry.getBuffer();
    memcpy(new_data, last_data, last_size);
    new_data[last_size] = next;
    this->table.push_back(entry);
}

void
Pl_LZWDecoder::handleCode(int code)
{
    if (this->eod)
    {
	return;
    }

    if (code == 256)
    {
	if (! this->table.empty())
	{
	    QTC::TC("libtests", "Pl_LZWDecoder intermediate reset");
	}
	this->table.clear();
	this->code_size = 9;
    }
    else if (code == 257)
    {
	this->eod = true;
    }
    else
    {
	if (this->last_code != 256)
	{
	    // Add to the table from last time.  New table entry would
	    // be what we read last plus the first character of what
	    // we're reading now.
	    unsigned char next = '\0';
	    unsigned int table_size = table.size();
	    if (code < 256)
	    {
		// just read < 256; last time's next was code
		next = code;
	    }
	    else if (code > 257)
	    {
		size_t idx = code - 258;
		if (idx > table_size)
		{
		    throw std::runtime_error("LZWDecoder: bad code received");
		}
		else if (idx == table_size)
		{
		    // The encoder would have just created this entry,
		    // so the first character of this entry would have
		    // been the same as the first character of the
		    // last entry.
		    QTC::TC("libtests", "Pl_LZWDecoder last was table size");
		    next = getFirstChar(this->last_code);
		}
		else
		{
		    next = getFirstChar(code);
		}
	    }
	    unsigned int new_idx = 258 + table_size;
	    if (new_idx == 4096)
	    {
		throw std::runtime_error("LZWDecoder: table full");
	    }
	    addToTable(next);
	    unsigned int change_idx = new_idx + code_change_delta;
	    if ((change_idx == 511) ||
		(change_idx == 1023) ||
		(change_idx == 2047))
	    {
		++this->code_size;
	    }
	}

	if (code < 256)
	{
	    unsigned char ch = static_cast<unsigned char>(code);
	    getNext()->write(&ch, 1);
	}
	else
	{
	    Buffer& b = table.at(code - 258);
	    getNext()->write(b.getBuffer(), b.getSize());
	}
    }

    this->last_code = code;
}

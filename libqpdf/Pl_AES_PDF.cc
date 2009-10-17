#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <assert.h>
#include <stdexcept>

Pl_AES_PDF::Pl_AES_PDF(char const* identifier, Pipeline* next,
		       bool encrypt, unsigned char* key_data) :
    Pipeline(identifier, next),
    encrypt(encrypt),
    offset(0)
{
    std::memset(this->buf, 0, this->buf_size);
    // XXX init
}

Pl_AES_PDF::~Pl_AES_PDF()
{
    // XXX finalize
}

void
Pl_AES_PDF::write(unsigned char* data, int len)
{
    unsigned int bytes_left = len;
    unsigned char* p = data;

    while (bytes_left > 0)
    {
	if (this->offset == this->buf_size)
	{
	    flush(false);
	}

	unsigned int available = this->buf_size - this->offset;
	int bytes = (bytes_left < available ? bytes_left : available);
	bytes_left -= bytes;
	std::memcpy(this->buf + this->offset, p, bytes);
	this->offset += bytes;
	p += bytes;
    }
}

void
Pl_AES_PDF::finish()
{
    if (this->encrypt)
    {
	if (this->offset == this->buf_size)
	{
	    flush(false);
	}
	// Pad as described in section 3.5.1 of version 1.7 of the PDF
	// specification, including providing an entire block of padding
	// if the input was a multiple of 16 bytes.
	unsigned char pad = this->buf_size - this->offset;
	memset(this->buf + this->offset, pad, pad);
	this->offset = this->buf_size;
	flush(false);
    }
    else
    {
	if (this->offset != this->buf_size)
	{
	    throw std::runtime_error(
		"aes encrypted stream length was not a multiple of " +
		QUtil::int_to_string(this->buf_size) + " bytes (offset = " +
		QUtil::int_to_string(this->offset) + ")");
	}
	flush(true);
    }
    getNext()->finish();
}

void
Pl_AES_PDF::flush(bool strip_padding)
{
    assert(this->offset == this->buf_size);
    if (this->encrypt)
    {
	// XXX encrypt this->buf
    }
    else
    {
	// XXX decrypt this->buf
    }
    unsigned int bytes = this->buf_size;
    if (strip_padding)
    {
	unsigned char last = this->buf[this->buf_size - 1];
	if (last <= this->buf_size)
	{
	    bool strip = true;
	    for (unsigned int i = 1; i <= last; ++i)
	    {
		if (this->buf[this->buf_size - i] != last)
		{
		    strip = false;
		    break;
		}
	    }
	    if (strip)
	    {
		bytes -= last;
	    }
	}
    }
    getNext()->write(this->buf, bytes);
    this->offset = 0;
}

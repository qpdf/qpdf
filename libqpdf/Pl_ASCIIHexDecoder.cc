#include <qpdf/Pl_ASCIIHexDecoder.hh>
#include <qpdf/QTC.hh>
#include <stdexcept>
#include <string.h>
#include <ctype.h>

Pl_ASCIIHexDecoder::Pl_ASCIIHexDecoder(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    pos(0),
    eod(false)
{
    this->inbuf[0] = '0';
    this->inbuf[1] = '0';
    this->inbuf[2] = '\0';
}

Pl_ASCIIHexDecoder::~Pl_ASCIIHexDecoder()
{
}

void
Pl_ASCIIHexDecoder::write(unsigned char* buf, size_t len)
{
    if (this->eod)
    {
	return;
    }
    for (size_t i = 0; i < len; ++i)
    {
	char ch = toupper(buf[i]);
	switch (ch)
	{
	  case ' ':
	  case '\f':
	  case '\v':
	  case '\t':
	  case '\r':
	  case '\n':
	    QTC::TC("libtests", "Pl_ASCIIHexDecoder ignore space");
	    // ignore whitespace
	    break;

	  case '>':
	    this->eod = true;
	    flush();
	    break;

	  default:
	    if (((ch >= '0') && (ch <= '9')) ||
		((ch >= 'A') && (ch <= 'F')))
	    {
		this->inbuf[this->pos++] = ch;
		if (this->pos == 2)
		{
		    flush();
		}
	    }
	    else
	    {
		char t[2];
		t[0] = ch;
		t[1] = 0;
		throw std::runtime_error(
		    std::string("character out of range"
				" during base Hex decode: ") + t);
	    }
	    break;
	}
	if (this->eod)
	{
	    break;
	}
    }
}

void
Pl_ASCIIHexDecoder::flush()
{
    if (this->pos == 0)
    {
	QTC::TC("libtests", "Pl_ASCIIHexDecoder no-op flush");
	return;
    }
    int b[2];
    for (int i = 0; i < 2; ++i)
    {
	if (this->inbuf[i] >= 'A')
	{
	    b[i] = this->inbuf[i] - 'A' + 10;
	}
	else
	{
	    b[i] = this->inbuf[i] - '0';
	}
    }
    unsigned char ch = static_cast<unsigned char>((b[0] << 4) + b[1]);

    QTC::TC("libtests", "Pl_ASCIIHexDecoder partial flush",
	    (this->pos == 2) ? 0 : 1);
    getNext()->write(&ch, 1);

    this->pos = 0;
    this->inbuf[0] = '0';
    this->inbuf[1] = '0';
    this->inbuf[2] = '\0';
}

void
Pl_ASCIIHexDecoder::finish()
{
    flush();
    getNext()->finish();
}

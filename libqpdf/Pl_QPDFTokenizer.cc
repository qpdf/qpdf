#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QTC.hh>
#include <stdexcept>
#include <string.h>

Pl_QPDFTokenizer::Pl_QPDFTokenizer(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    newline_after_next_token(false),
    just_wrote_nl(false),
    last_char_was_cr(false),
    unread_char(false),
    char_to_unread('\0'),
    in_inline_image(false)
{
    memset(this->image_buf, 0, IMAGE_BUF_SIZE);
}

Pl_QPDFTokenizer::~Pl_QPDFTokenizer()
{
}

void
Pl_QPDFTokenizer::writeNext(char const* buf, size_t len)
{
    if (len)
    {
	unsigned char* t = new unsigned char[len];
	memcpy(t, buf, len);
	getNext()->write(t, len);
	delete [] t;
	this->just_wrote_nl = (buf[len-1] == '\n');
    }
}

void
Pl_QPDFTokenizer::writeToken(QPDFTokenizer::Token& token)
{
    std::string value = token.getRawValue();

    switch (token.getType())
    {
      case QPDFTokenizer::tt_string:
	value = QPDF_String(token.getValue()).unparse();
	break;

      case QPDFTokenizer::tt_name:
	value = QPDF_Name(token.getValue()).unparse();
	break;

      default:
	break;
    }
    writeNext(value.c_str(), value.length());
}

void
Pl_QPDFTokenizer::processChar(char ch)
{
    if (this->in_inline_image)
    {
	// Scan through the input looking for EI surrounded by
	// whitespace.  If that pattern appears in the inline image's
	// representation, we're hosed, but this situation seems
	// excessively unlikely, and this code path is only followed
	// during content stream normalization, which is pretty much
	// used for debugging and human inspection of PDF files.
	memmove(this->image_buf,
		this->image_buf + 1,
		IMAGE_BUF_SIZE - 1);
	this->image_buf[IMAGE_BUF_SIZE - 1] = ch;
	if (strchr(" \t\n\v\f\r", this->image_buf[0]) &&
	    (this->image_buf[1] == 'E') &&
	    (this->image_buf[2] == 'I') &&
	    strchr(" \t\n\v\f\r", this->image_buf[3]))
	{
	    // We've found an EI operator.  We've already written the
	    // EI operator to output; terminate with a newline
	    // character and resume normal processing.
	    writeNext("\n", 1);
	    this->in_inline_image = false;
	    QTC::TC("qpdf", "Pl_QPDFTokenizer found EI");
	}
	else
	{
	    writeNext(&ch, 1);
	}
	return;
    }

    tokenizer.presentCharacter(ch);
    QPDFTokenizer::Token token;
    if (tokenizer.getToken(token, this->unread_char, this->char_to_unread))
    {
	writeToken(token);
	if (this->newline_after_next_token)
	{
	    writeNext("\n", 1);
	    this->newline_after_next_token = false;
	}
	if ((token.getType() == QPDFTokenizer::tt_word) &&
	    (token.getValue() == "ID"))
	{
	    // Suspend normal scanning until we find an EI token.
	    this->in_inline_image = true;
	    if (this->unread_char)
	    {
		writeNext(&this->char_to_unread, 1);
		this->unread_char = false;
	    }
	}
    }
    else
    {
	bool suppress = false;
	if ((ch == '\n') && (this->last_char_was_cr))
	{
	    // Always ignore \n following \r
	    suppress = true;
	}

	if ((this->last_char_was_cr = (ch == '\r')))
	{
	    ch = '\n';
	}

	if (this->tokenizer.betweenTokens())
	{
	    if (! suppress)
	    {
		writeNext(&ch, 1);
	    }
	}
	else
	{
	    if (ch == '\n')
	    {
		this->newline_after_next_token = true;
	    }
	}
    }
}


void
Pl_QPDFTokenizer::checkUnread()
{
    if (this->unread_char)
    {
	processChar(this->char_to_unread);
	if (this->unread_char)
	{
	    throw std::logic_error(
		"INTERNAL ERROR: unread_char still true after processing "
		"unread character");
	}
    }
}

void
Pl_QPDFTokenizer::write(unsigned char* buf, size_t len)
{
    checkUnread();
    for (size_t i = 0; i < len; ++i)
    {
	processChar(buf[i]);
	checkUnread();
    }
}

void
Pl_QPDFTokenizer::finish()
{
    this->tokenizer.presentEOF();
    if (! this->in_inline_image)
    {
	QPDFTokenizer::Token token;
	if (tokenizer.getToken(token, this->unread_char, this->char_to_unread))
	{
	    writeToken(token);
	    if (unread_char)
	    {
		if (this->char_to_unread == '\r')
		{
		    this->char_to_unread = '\n';
		}
		writeNext(&this->char_to_unread, 1);
	    }
	}
    }
    if (! this->just_wrote_nl)
    {
	writeNext("\n", 1);
    }

    getNext()->finish();
}

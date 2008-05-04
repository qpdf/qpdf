
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Name.hh>
#include <string.h>

Pl_QPDFTokenizer::Pl_QPDFTokenizer(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    newline_after_next_token(false),
    just_wrote_nl(false),
    last_char_was_cr(false),
    unread_char(false),
    char_to_unread('\0'),
    pass_through(false)
{
}

Pl_QPDFTokenizer::~Pl_QPDFTokenizer()
{
}

void
Pl_QPDFTokenizer::writeNext(char const* buf, int len)
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
    if (this->pass_through)
    {
	// We're not noramlizing anymore -- just write this without
	// looking at it.
	writeNext(&ch, 1);
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
	    (token.getValue() == "BI"))
	{
	    // Uh oh.... we're not sophisticated enough to handle
	    // inline images safely.  We'd have to to set up all the
	    // filters and pipe the iamge data through it until the
	    // filtered output was the right size for an image of the
	    // specified dimensions.  Then we'd either have to write
	    // out raw image data or continue to write filtered data,
	    // resuming normalization when we get to the end.
	    // Insetad, for now, we'll just turn off noramlization for
	    // the remainder of this stream.
	    this->pass_through = true;
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
	    throw QEXC::Internal("unread_char still true after processing "
				 "unread character");
	}
    }
}

void
Pl_QPDFTokenizer::write(unsigned char* buf, int len)
{
    checkUnread();
    for (int i = 0; i < len; ++i)
    {
	processChar(buf[i]);
	checkUnread();
    }
}

void
Pl_QPDFTokenizer::finish()
{
    this->tokenizer.presentEOF();
    if (! this->pass_through)
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

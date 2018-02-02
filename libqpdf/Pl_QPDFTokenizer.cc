#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QTC.hh>
#include <stdexcept>
#include <string.h>

Pl_QPDFTokenizer::Members::Members() :
    filter(0),
    last_char_was_cr(false),
    unread_char(false),
    char_to_unread('\0')
{
}

Pl_QPDFTokenizer::Members::~Members()
{
}

Pl_QPDFTokenizer::Pl_QPDFTokenizer(
    char const* identifier,
    QPDFObjectHandle::TokenFilter* filter)
    :
    Pipeline(identifier, 0),
    m(new Members)
{
    m->filter = filter;
    m->tokenizer.allowEOF();
    m->tokenizer.includeIgnorable();
}

Pl_QPDFTokenizer::~Pl_QPDFTokenizer()
{
}

void
Pl_QPDFTokenizer::processChar(char ch)
{
    this->m->tokenizer.presentCharacter(ch);
    QPDFTokenizer::Token token;
    if (this->m->tokenizer.getToken(
            token, this->m->unread_char, this->m->char_to_unread))
    {
	this->m->filter->handleToken(token);
        if ((token.getType() == QPDFTokenizer::tt_word) &&
            (token.getValue() == "ID"))
        {
            QTC::TC("qpdf", "Pl_QPDFTokenizer found ID");
            this->m->tokenizer.expectInlineImage();
        }
    }
}


void
Pl_QPDFTokenizer::checkUnread()
{
    if (this->m->unread_char)
    {
	processChar(this->m->char_to_unread);
	if (this->m->unread_char)
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
    this->m->tokenizer.presentEOF();
    QPDFTokenizer::Token token;
    if (this->m->tokenizer.getToken(
            token, this->m->unread_char, this->m->char_to_unread))
    {
	this->m->filter->handleToken(token);
    }

    this->m->filter->handleEOF();
}

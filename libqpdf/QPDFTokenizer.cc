#include <qpdf/QPDFTokenizer.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.

#include <qpdf/QTC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <string.h>
#include <cstdlib>

QPDFTokenizer::Members::Members() :
    pound_special_in_name(true),
    allow_eof(false),
    include_ignorable(false)
{
    reset();
}

void
QPDFTokenizer::Members::reset()
{
    state = st_top;
    type = tt_bad;
    val = "";
    raw_val = "";
    error_message = "";
    unread_char = false;
    char_to_unread = '\0';
    string_depth = 0;
    string_ignoring_newline = false;
    last_char_was_bs = false;
}

QPDFTokenizer::Members::~Members()
{
}

QPDFTokenizer::QPDFTokenizer() :
    m(new Members())
{
}

void
QPDFTokenizer::allowPoundAnywhereInName()
{
    QTC::TC("qpdf", "QPDFTokenizer allow pound anywhere in name");
    this->m->pound_special_in_name = false;
}

void
QPDFTokenizer::allowEOF()
{
    this->m->allow_eof = true;
}

void
QPDFTokenizer::includeIgnorable()
{
    this->m->include_ignorable = true;
}

bool
QPDFTokenizer::isSpace(char ch)
{
    return ((ch == '\0') || QUtil::is_space(ch));
}

bool
QPDFTokenizer::isDelimiter(char ch)
{
    return (strchr(" \t\n\v\f\r()<>[]{}/%", ch) != 0);
}

void
QPDFTokenizer::resolveLiteral()
{
    if ((this->m->val.length() > 0) && (this->m->val.at(0) == '/'))
    {
        this->m->type = tt_name;
        // Deal with # in name token.  Note: '/' by itself is a
        // valid name, so don't strip leading /.  That way we
        // don't have to deal with the empty string as a name.
        std::string nval = "/";
        char const* valstr = this->m->val.c_str() + 1;
        for (char const* p = valstr; *p; ++p)
        {
            if ((*p == '#') && this->m->pound_special_in_name)
            {
                if (p[1] && p[2] &&
                    QUtil::is_hex_digit(p[1]) && QUtil::is_hex_digit(p[2]))
                {
                    char num[3];
                    num[0] = p[1];
                    num[1] = p[2];
                    num[2] = '\0';
                    char ch = static_cast<char>(strtol(num, 0, 16));
                    if (ch == '\0')
                    {
                        this->m->type = tt_bad;
                        QTC::TC("qpdf", "QPDFTokenizer null in name");
                        this->m->error_message =
                            "null character not allowed in name token";
                        nval += "#00";
                    }
                    else
                    {
                        nval += ch;
                    }
                    p += 2;
                }
                else
                {
                    QTC::TC("qpdf", "QPDFTokenizer bad name");
                    this->m->type = tt_bad;
                    this->m->error_message = "invalid name token";
                    nval += *p;
                }
            }
            else
            {
                nval += *p;
            }
        }
        this->m->val = nval;
    }
    else if (QUtil::is_number(this->m->val.c_str()))
    {
        if (this->m->val.find('.') != std::string::npos)
        {
            this->m->type = tt_real;
        }
        else
        {
            this->m->type = tt_integer;
        }
    }
    else if ((this->m->val == "true") || (this->m->val == "false"))
    {
        this->m->type = tt_bool;
    }
    else if (this->m->val == "null")
    {
        this->m->type = tt_null;
    }
    else
    {
        // I don't really know what it is, so leave it as tt_word.
        // Lots of cases ($, #, etc.) other than actual words fall
        // into this category, but that's okay at least for now.
        this->m->type = tt_word;
    }
}

void
QPDFTokenizer::presentCharacter(char ch)
{
    if (this->m->state == st_token_ready)
    {
	throw std::logic_error(
	    "INTERNAL ERROR: QPDF tokenizer presented character "
	    "while token is waiting");
    }

    char orig_ch = ch;

    // State machine is implemented such that some characters may be
    // handled more than once.  This happens whenever you have to use
    // the character that caused a state change in the new state.

    bool handled = true;
    if (this->m->state == st_top)
    {
	// Note: we specifically do not use ctype here.  It is
	// locale-dependent.
	if (isSpace(ch))
	{
            if (this->m->include_ignorable)
            {
                this->m->state = st_in_space;
                this->m->val += ch;
            }
	}
	else if (ch == '%')
	{
	    this->m->state = st_in_comment;
            if (this->m->include_ignorable)
            {
                this->m->val += ch;
            }
	}
	else if (ch == '(')
	{
	    this->m->string_depth = 1;
	    this->m->string_ignoring_newline = false;
	    memset(this->m->bs_num_register, '\0',
                   sizeof(this->m->bs_num_register));
	    this->m->last_char_was_bs = false;
	    this->m->state = st_in_string;
	}
	else if (ch == '<')
	{
	    this->m->state = st_lt;
	}
	else if (ch == '>')
	{
	    this->m->state = st_gt;
	}
	else
	{
	    this->m->val += ch;
	    if (ch == ')')
	    {
		this->m->type = tt_bad;
		QTC::TC("qpdf", "QPDFTokenizer bad )");
		this->m->error_message = "unexpected )";
		this->m->state = st_token_ready;
	    }
	    else if (ch == '[')
	    {
		this->m->type = tt_array_open;
		this->m->state = st_token_ready;
	    }
	    else if (ch == ']')
	    {
		this->m->type = tt_array_close;
		this->m->state = st_token_ready;
	    }
	    else if (ch == '{')
	    {
		this->m->type = tt_brace_open;
		this->m->state = st_token_ready;
	    }
	    else if (ch == '}')
	    {
		this->m->type = tt_brace_close;
		this->m->state = st_token_ready;
	    }
	    else
	    {
		this->m->state = st_literal;
	    }
	}
    }
    else if (this->m->state == st_in_space)
    {
        // We only enter this state if include_ignorable is true.
        if (! isSpace(ch))
        {
	    this->m->type = tt_space;
	    this->m->unread_char = true;
	    this->m->char_to_unread = ch;
	    this->m->state = st_token_ready;
        }
        else
        {
            this->m->val += ch;
        }
    }
    else if (this->m->state == st_in_comment)
    {
	if ((ch == '\r') || (ch == '\n'))
        {
            if (this->m->include_ignorable)
            {
                this->m->type = tt_comment;
                this->m->unread_char = true;
                this->m->char_to_unread = ch;
                this->m->state = st_token_ready;
            }
            else
            {
                this->m->state = st_top;
            }
        }
        else if (this->m->include_ignorable)
        {
            this->m->val += ch;
        }
    }
    else if (this->m->state == st_lt)
    {
	if (ch == '<')
	{
	    this->m->val = "<<";
	    this->m->type = tt_dict_open;
	    this->m->state = st_token_ready;
	}
	else
	{
	    handled = false;
	    this->m->state = st_in_hexstring;
	}
    }
    else if (this->m->state == st_gt)
    {
	if (ch == '>')
	{
	    this->m->val = ">>";
	    this->m->type = tt_dict_close;
	    this->m->state = st_token_ready;
	}
	else
	{
	    this->m->val = ">";
	    this->m->type = tt_bad;
	    QTC::TC("qpdf", "QPDFTokenizer bad >");
	    this->m->error_message = "unexpected >";
	    this->m->unread_char = true;
	    this->m->char_to_unread = ch;
	    this->m->state = st_token_ready;
	}
    }
    else if (this->m->state == st_in_string)
    {
	if (this->m->string_ignoring_newline &&
            (! ((ch == '\r') || (ch == '\n'))))
	{
	    this->m->string_ignoring_newline = false;
	}

	size_t bs_num_count = strlen(this->m->bs_num_register);
	bool ch_is_octal = ((ch >= '0') && (ch <= '7'));
	if ((bs_num_count == 3) || ((bs_num_count > 0) && (! ch_is_octal)))
	{
	    // We've accumulated \ddd.  PDF Spec says to ignore
	    // high-order overflow.
	    this->m->val += static_cast<char>(
                strtol(this->m->bs_num_register, 0, 8));
	    memset(this->m->bs_num_register, '\0',
                   sizeof(this->m->bs_num_register));
	    bs_num_count = 0;
	}

	if (this->m->string_ignoring_newline && ((ch == '\r') || (ch == '\n')))
	{
	    // ignore
	}
	else if (ch_is_octal &&
                 (this->m->last_char_was_bs || (bs_num_count > 0)))
	{
	    this->m->bs_num_register[bs_num_count++] = ch;
	}
	else if (this->m->last_char_was_bs)
	{
	    switch (ch)
	    {
	      case 'n':
		this->m->val += '\n';
		break;

	      case 'r':
		this->m->val += '\r';
		break;

	      case 't':
		this->m->val += '\t';
		break;

	      case 'b':
		this->m->val += '\b';
		break;

	      case 'f':
		this->m->val += '\f';
		break;

	      case '\r':
	      case '\n':
		this->m->string_ignoring_newline = true;
		break;

	      default:
		// PDF spec says backslash is ignored before anything else
		this->m->val += ch;
		break;
	    }
	}
	else if (ch == '\\')
	{
	    // last_char_was_bs is set/cleared below as appropriate
	    if (bs_num_count)
	    {
		throw std::logic_error(
		    "INTERNAL ERROR: QPDFTokenizer: bs_num_count != 0 "
		    "when ch == '\\'");
	    }
	}
	else if (ch == '(')
	{
	    this->m->val += ch;
	    ++this->m->string_depth;
	}
	else if ((ch == ')') && (--this->m->string_depth == 0))
	{
	    this->m->type = tt_string;
	    this->m->state = st_token_ready;
	}
	else
	{
	    this->m->val += ch;
	}

	this->m->last_char_was_bs =
            ((! this->m->last_char_was_bs) && (ch == '\\'));
    }
    else if (this->m->state == st_literal)
    {
	if (isDelimiter(ch))
	{
	    // A C-locale whitespace character or delimiter terminates
	    // token.  It is important to unread the whitespace
	    // character even though it is ignored since it may be the
	    // newline after a stream keyword.  Removing it here could
	    // make the stream-reading code break on some files,
	    // though not on any files in the test suite as of this
	    // writing.

	    this->m->type = tt_word;
	    this->m->unread_char = true;
	    this->m->char_to_unread = ch;
	    this->m->state = st_token_ready;
	}
	else
	{
	    this->m->val += ch;
	}
    }
    else if (this->m->state == st_inline_image)
    {
        size_t len = this->m->val.length();
        if ((len >= 4) &&
            isDelimiter(this->m->val.at(len-4)) &&
            (this->m->val.at(len-3) == 'E') &&
            (this->m->val.at(len-2) == 'I') &&
            isDelimiter(this->m->val.at(len-1)))
        {
            this->m->type = tt_inline_image;
            this->m->unread_char = true;
            this->m->char_to_unread = ch;
            this->m->state = st_token_ready;
        }
        else
        {
            this->m->val += ch;
        }
    }
    else
    {
	handled = false;
    }


    if (handled)
    {
	// okay
    }
    else if (this->m->state == st_in_hexstring)
    {
	if (ch == '>')
	{
	    this->m->type = tt_string;
	    this->m->state = st_token_ready;
	    if (this->m->val.length() % 2)
	    {
		// PDF spec says odd hexstrings have implicit
		// trailing 0.
		this->m->val += '0';
	    }
	    char num[3];
	    num[2] = '\0';
	    std::string nval;
	    for (unsigned int i = 0; i < this->m->val.length(); i += 2)
	    {
		num[0] = this->m->val.at(i);
		num[1] = this->m->val.at(i+1);
		char nch = static_cast<char>(strtol(num, 0, 16));
		nval += nch;
	    }
	    this->m->val = nval;
	}
	else if (QUtil::is_hex_digit(ch))
	{
	    this->m->val += ch;
	}
	else if (isSpace(ch))
	{
	    // ignore
	}
	else
	{
	    this->m->type = tt_bad;
	    QTC::TC("qpdf", "QPDFTokenizer bad hexstring character");
	    this->m->error_message = std::string("invalid character (") +
		ch + ") in hexstring";
	    this->m->state = st_token_ready;
	}
    }
    else
    {
	throw std::logic_error(
	    "INTERNAL ERROR: invalid state while reading token");
    }

    if ((this->m->state == st_token_ready) && (this->m->type == tt_word))
    {
        resolveLiteral();
    }

    if (! (betweenTokens() ||
           ((this->m->state == st_token_ready) && this->m->unread_char)))
    {
	this->m->raw_val += orig_ch;
    }
}

void
QPDFTokenizer::presentEOF()
{
    if (this->m->state == st_inline_image)
    {
        size_t len = this->m->val.length();
        if ((len >= 3) &&
            isDelimiter(this->m->val.at(len-3)) &&
            (this->m->val.at(len-2) == 'E') &&
            (this->m->val.at(len-1) == 'I'))
        {
            QTC::TC("qpdf", "QPDFTokenizer inline image at EOF");
            this->m->type = tt_inline_image;
            this->m->state = st_token_ready;
        }
    }

    if (this->m->state == st_literal)
    {
        QTC::TC("qpdf", "QPDFTokenizer EOF reading appendable token");
        resolveLiteral();
    }
    else if ((this->m->include_ignorable) && (this->m->state == st_in_space))
    {
        this->m->type = tt_space;
    }
    else if ((this->m->include_ignorable) && (this->m->state == st_in_comment))
    {
        this->m->type = tt_comment;
    }
    else if (betweenTokens())
    {
        this->m->type = tt_eof;
    }
    else if (this->m->state != st_token_ready)
    {
        QTC::TC("qpdf", "QPDFTokenizer EOF reading token");
        this->m->type = tt_bad;
        this->m->error_message = "EOF while reading token";
    }

    this->m->state = st_token_ready;
}

void
QPDFTokenizer::expectInlineImage()
{
    if (this->m->state != st_top)
    {
        throw std::logic_error("QPDFTokenizer::expectInlineImage called"
                               " when tokenizer is in improper state");
    }
    this->m->state = st_inline_image;
}

bool
QPDFTokenizer::getToken(Token& token, bool& unread_char, char& ch)
{
    bool ready = (this->m->state == st_token_ready);
    unread_char = this->m->unread_char;
    ch = this->m->char_to_unread;
    if (ready)
    {
        if (this->m->type == tt_bad)
        {
            this->m->val = this->m->raw_val;
        }
	token = Token(this->m->type, this->m->val,
                      this->m->raw_val, this->m->error_message);
	this->m->reset();
    }
    return ready;
}

bool
QPDFTokenizer::betweenTokens()
{
    return ((this->m->state == st_top) ||
            ((! this->m->include_ignorable) &&
             ((this->m->state == st_in_comment) ||
              (this->m->state == st_in_space))));
}

QPDFTokenizer::Token
QPDFTokenizer::readToken(PointerHolder<InputSource> input,
                         std::string const& context,
                         bool allow_bad,
                         size_t max_len)
{
    qpdf_offset_t offset = input->tell();
    Token token;
    bool unread_char;
    char char_to_unread;
    bool presented_eof = false;
    while (! getToken(token, unread_char, char_to_unread))
    {
	char ch;
	if (input->read(&ch, 1) == 0)
	{
            if (! presented_eof)
            {
                presentEOF();
                presented_eof = true;
                if ((this->m->type == tt_eof) && (! this->m->allow_eof))
                {
                    QTC::TC("qpdf", "QPDFTokenizer EOF when not allowed");
                    this->m->type = tt_bad;
                    this->m->error_message = "unexpected EOF";
                    offset = input->getLastOffset();
                }
            }
            else
            {
                throw std::logic_error(
                    "getToken returned false after presenting EOF");
            }
	}
	else
	{
	    presentCharacter(ch);
	    if (betweenTokens() && (input->getLastOffset() == offset))
	    {
		++offset;
	    }
            if (max_len && (this->m->raw_val.length() >= max_len) &&
                (this->m->state != st_token_ready))
            {
                // terminate this token now
                QTC::TC("qpdf", "QPDFTokenizer block long token");
                this->m->type = tt_bad;
                this->m->state = st_token_ready;
                this->m->error_message =
                    "exceeded allowable length while reading token";
            }
	}
    }

    if (unread_char)
    {
	input->unreadCh(char_to_unread);
    }

    input->setLastOffset(offset);

    if (token.getType() == tt_bad)
    {
        if (allow_bad)
        {
            QTC::TC("qpdf", "QPDFTokenizer allowing bad token");
        }
        else
        {
            throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                          context, offset, token.getErrorMessage());
        }
    }

    return token;
}

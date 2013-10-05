#include <qpdf/QPDFTokenizer.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.

#include <qpdf/PCRE.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFExc.hh>

#include <stdexcept>
#include <string.h>

// See note above about ctype.
static bool is_hex_digit(char ch)
{
    return (strchr("0123456789abcdefABCDEF", ch) != 0);
}
static bool is_space(char ch)
{
    return (strchr(" \f\n\r\t\v", ch) != 0);
}

QPDFTokenizer::QPDFTokenizer() :
    pound_special_in_name(true),
    allow_eof(false)
{
    reset();
}

void
QPDFTokenizer::allowPoundAnywhereInName()
{
    QTC::TC("qpdf", "QPDFTokenizer allow pound anywhere in name");
    this->pound_special_in_name = false;
}

void
QPDFTokenizer::allowEOF()
{
    this->allow_eof = true;
}

void
QPDFTokenizer::reset()
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

void
QPDFTokenizer::resolveLiteral()
{
    PCRE num_re("^[\\+\\-]?(?:\\.\\d+|\\d+(?:\\.\\d+)?)$");

    if ((val.length() > 0) && (val.at(0) == '/'))
    {
        type = tt_name;
        // Deal with # in name token.  Note: '/' by itself is a
        // valid name, so don't strip leading /.  That way we
        // don't have to deal with the empty string as a name.
        std::string nval = "/";
        char const* valstr = val.c_str() + 1;
        for (char const* p = valstr; *p; ++p)
        {
            if ((*p == '#') && this->pound_special_in_name)
            {
                if (p[1] && p[2] &&
                    is_hex_digit(p[1]) && is_hex_digit(p[2]))
                {
                    char num[3];
                    num[0] = p[1];
                    num[1] = p[2];
                    num[2] = '\0';
                    char ch = static_cast<char>(strtol(num, 0, 16));
                    if (ch == '\0')
                    {
                        type = tt_bad;
                        QTC::TC("qpdf", "QPDF_Tokenizer null in name");
                        error_message =
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
                    QTC::TC("qpdf", "QPDF_Tokenizer bad name");
                    type = tt_bad;
                    error_message = "invalid name token";
                    nval += *p;
                }
            }
            else
            {
                nval += *p;
            }
        }
        val = nval;
    }
    else if (num_re.match(val.c_str()))
    {
        if (val.find('.') != std::string::npos)
        {
            type = tt_real;
        }
        else
        {
            type = tt_integer;
        }
    }
    else if ((val == "true") || (val == "false"))
    {
        type = tt_bool;
    }
    else if (val == "null")
    {
        type = tt_null;
    }
    else
    {
        // I don't really know what it is, so leave it as tt_word.
        // Lots of cases ($, #, etc.) other than actual words fall
        // into this category, but that's okay at least for now.
        type = tt_word;
    }
}

void
QPDFTokenizer::presentCharacter(char ch)
{
    if (state == st_token_ready)
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
    if (state == st_top)
    {
	// Note: we specifically do not use ctype here.  It is
	// locale-dependent.
	if (strchr(" \t\n\v\f\r", ch))
	{
	    // ignore
	}
	else if (ch == '%')
	{
	    // Discard comments
	    state = st_in_comment;
	}
	else if (ch == '(')
	{
	    string_depth = 1;
	    string_ignoring_newline = false;
	    memset(bs_num_register, '\0', sizeof(bs_num_register));
	    last_char_was_bs = false;
	    state = st_in_string;
	}
	else if (ch == '<')
	{
	    state = st_lt;
	}
	else if (ch == '>')
	{
	    state = st_gt;
	}
	else
	{
	    val += ch;
	    if (ch == ')')
	    {
		type = tt_bad;
		QTC::TC("qpdf", "QPDF_Tokenizer bad )");
		error_message = "unexpected )";
		state = st_token_ready;
	    }
	    else if (ch == '[')
	    {
		type = tt_array_open;
		state = st_token_ready;
	    }
	    else if (ch == ']')
	    {
		type = tt_array_close;
		state = st_token_ready;
	    }
	    else if (ch == '{')
	    {
		type = tt_brace_open;
		state = st_token_ready;
	    }
	    else if (ch == '}')
	    {
		type = tt_brace_close;
		state = st_token_ready;
	    }
	    else
	    {
		state = st_literal;
	    }
	}
    }
    else if (state == st_in_comment)
    {
	if ((ch == '\r') || (ch == '\n'))
	{
	    state = st_top;
	}
    }
    else if (state == st_lt)
    {
	if (ch == '<')
	{
	    val = "<<";
	    type = tt_dict_open;
	    state = st_token_ready;
	}
	else
	{
	    handled = false;
	    state = st_in_hexstring;
	}
    }
    else if (state == st_gt)
    {
	if (ch == '>')
	{
	    val = ">>";
	    type = tt_dict_close;
	    state = st_token_ready;
	}
	else
	{
	    val = ">";
	    type = tt_bad;
	    QTC::TC("qpdf", "QPDF_Tokenizer bad >");
	    error_message = "unexpected >";
	    unread_char = true;
	    char_to_unread = ch;
	    state = st_token_ready;
	}
    }
    else if (state == st_in_string)
    {
	if (string_ignoring_newline && (! ((ch == '\r') || (ch == '\n'))))
	{
	    string_ignoring_newline = false;
	}

	size_t bs_num_count = strlen(bs_num_register);
	bool ch_is_octal = ((ch >= '0') && (ch <= '7'));
	if ((bs_num_count == 3) || ((bs_num_count > 0) && (! ch_is_octal)))
	{
	    // We've accumulated \ddd.  PDF Spec says to ignore
	    // high-order overflow.
	    val += static_cast<char>(strtol(bs_num_register, 0, 8));
	    memset(bs_num_register, '\0', sizeof(bs_num_register));
	    bs_num_count = 0;
	}

	if (string_ignoring_newline && ((ch == '\r') || (ch == '\n')))
	{
	    // ignore
	}
	else if (ch_is_octal && (last_char_was_bs || (bs_num_count > 0)))
	{
	    bs_num_register[bs_num_count++] = ch;
	}
	else if (last_char_was_bs)
	{
	    switch (ch)
	    {
	      case 'n':
		val += '\n';
		break;

	      case 'r':
		val += '\r';
		break;

	      case 't':
		val += '\t';
		break;

	      case 'b':
		val += '\b';
		break;

	      case 'f':
		val += '\f';
		break;

	      case '\r':
	      case '\n':
		string_ignoring_newline = true;
		break;

	      default:
		// PDF spec says backslash is ignored before anything else
		val += ch;
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
	    val += ch;
	    ++string_depth;
	}
	else if ((ch == ')') && (--string_depth == 0))
	{
	    type = tt_string;
	    state = st_token_ready;
	}
	else
	{
	    val += ch;
	}

	last_char_was_bs = ((! last_char_was_bs) && (ch == '\\'));
    }
    else if (state == st_literal)
    {
	if (strchr(" \t\n\v\f\r()<>[]{}/%", ch) != 0)
	{
	    // A C-locale whitespace character or delimiter terminates
	    // token.  It is important to unread the whitespace
	    // character even though it is ignored since it may be the
	    // newline after a stream keyword.  Removing it here could
	    // make the stream-reading code break on some files,
	    // though not on any files in the test suite as of this
	    // writing.

	    type = tt_word;
	    unread_char = true;
	    char_to_unread = ch;
	    state = st_token_ready;
	}
	else
	{
	    val += ch;
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
    else if (state == st_in_hexstring)
    {
	if (ch == '>')
	{
	    type = tt_string;
	    state = st_token_ready;
	    if (val.length() % 2)
	    {
		// PDF spec says odd hexstrings have implicit
		// trailing 0.
		val += '0';
	    }
	    char num[3];
	    num[2] = '\0';
	    std::string nval;
	    for (unsigned int i = 0; i < val.length(); i += 2)
	    {
		num[0] = val.at(i);
		num[1] = val.at(i+1);
		char nch = static_cast<char>(strtol(num, 0, 16));
		nval += nch;
	    }
	    val = nval;
	}
	else if (is_hex_digit(ch))
	{
	    val += ch;
	}
	else if (strchr(" \t\n\v\f\r", ch))
	{
	    // ignore
	}
	else
	{
	    type = tt_bad;
	    QTC::TC("qpdf", "QPDF_Tokenizer bad (");
	    error_message = std::string("invalid character (") +
		ch + ") in hexstring";
	    state = st_token_ready;
	}
    }
    else
    {
	throw std::logic_error(
	    "INTERNAL ERROR: invalid state while reading token");
    }

    if ((state == st_token_ready) && (type == tt_word))
    {
        resolveLiteral();
    }

    if (! (betweenTokens() || ((state == st_token_ready) && unread_char)))
    {
	this->raw_val += orig_ch;
    }
}

void
QPDFTokenizer::presentEOF()
{
    if (state == st_literal)
    {
        QTC::TC("qpdf", "QPDF_Tokenizer EOF reading appendable token");
        resolveLiteral();
    }
    else if (state != st_token_ready)
    {
        QTC::TC("qpdf", "QPDF_Tokenizer EOF reading token",
                this->allow_eof ? 1 : 0);
        if (this->allow_eof)
        {
            type = tt_eof;
        }
        else
        {
            type = tt_bad;
            error_message = "EOF while reading token";
        }
    }

    state = st_token_ready;
}

bool
QPDFTokenizer::getToken(Token& token, bool& unread_char, char& ch)
{
    bool ready = (this->state == st_token_ready);
    unread_char = this->unread_char;
    ch = this->char_to_unread;
    if (ready)
    {
	token = Token(type, val, raw_val, error_message);
	reset();
    }
    return ready;
}

bool
QPDFTokenizer::betweenTokens()
{
    return ((state == st_top) || (state == st_in_comment));
}

QPDFTokenizer::Token
QPDFTokenizer::readToken(PointerHolder<InputSource> input,
                         std::string const& context)
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
            }
            else
            {
                throw std::logic_error(
                    "getToken returned false after presenting EOF");
            }
	}
	else
	{
	    if (is_space(static_cast<unsigned char>(ch)) &&
		(input->getLastOffset() == offset))
	    {
		++offset;
	    }
	    presentCharacter(ch);
	}
    }

    if (unread_char)
    {
	input->unreadCh(char_to_unread);
    }

    if (token.getType() == tt_bad)
    {
	throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
		      context, offset, token.getErrorMessage());
    }

    input->setLastOffset(offset);

    return token;
}

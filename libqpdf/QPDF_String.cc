#include <qpdf/QPDF_String.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.
#include <string.h>

// See above about ctype.
static bool is_ascii_printable(unsigned char ch)
{
    return ((ch >= 32) && (ch <= 126));
}
static bool is_iso_latin1_printable(unsigned char ch)
{
    return (((ch >= 32) && (ch <= 126)) || (ch >= 160));
}

QPDF_String::QPDF_String(std::string const& val) :
    val(val)
{
}

QPDF_String::~QPDF_String()
{
}

std::string
QPDF_String::unparse()
{
    return unparse(false);
}

std::string
QPDF_String::unparse(bool force_binary)
{
    bool use_hexstring = force_binary;
    if (! use_hexstring)
    {
	unsigned int nonprintable = 0;
	int consecutive_printable = 0;
	for (unsigned int i = 0; i < this->val.length(); ++i)
	{
	    char ch = this->val[i];
	    // Note: do not use locale to determine printability.  The
	    // PDF specification accepts arbitrary binary data.  Some
	    // locales imply multibyte characters.  We'll consider
	    // something printable if it is printable in 7-bit ASCII.
	    // We'll code this manually rather than being rude and
	    // setting locale.
	    if ((ch == 0) || (! (is_ascii_printable(ch) ||
				 strchr("\n\r\t\b\f", ch))))
	    {
		++nonprintable;
		consecutive_printable = 0;
	    }
	    else
	    {
		if (++consecutive_printable > 5)
		{
		    // If there are more than 5 consecutive printable
		    // characters, I want to see them as such.
		    nonprintable = 0;
		    break;
		}
	    }
	}

	// Use hex notation if more than 20% of the characters are not
	// printable in plain ASCII.
	if (5 * nonprintable > val.length())
	{
	    use_hexstring = true;
	}
    }
    std::string result;
    if (use_hexstring)
    {
	result += "<";
	char num[3];
	for (unsigned int i = 0; i < this->val.length(); ++i)
	{
	    sprintf(num, "%02x", (unsigned char) this->val[i]);
	    result += num;
	}
	result += ">";
    }
    else
    {
	result += "(";
	char num[5];
	for (unsigned int i = 0; i < this->val.length(); ++i)
	{
	    char ch = this->val[i];
	    switch (ch)
	    {
	      case '\n':
		result += "\\n";
		break;

	      case '\r':
		result += "\\r";
		break;

	      case '\t':
		result += "\\t";
		break;

	      case '\b':
		result += "\\b";
		break;

	      case '\f':
		result += "\\f";
		break;

	      case '(':
		result += "\\(";
		break;

	      case ')':
		result += "\\)";
		break;

	      case '\\':
		result += "\\\\";
		break;

	      default:
		if (is_iso_latin1_printable(ch))
		{
		    result += this->val[i];
		}
		else
		{
		    sprintf(num, "\\%03o", (unsigned char)ch);
		    result += num;
		}
		break;
	    }
	}
	result += ")";
    }

    return result;
}

std::string
QPDF_String::getVal() const
{
    return this->val;
}

std::string
QPDF_String::getUTF8Val() const
{
    std::string result;
    size_t len = this->val.length();
    if ((len >= 2) && (len % 2 == 0) &&
	(this->val[0] == '\xfe') && (this->val[1] == '\xff'))
    {
	// This is a Unicode string using big-endian UTF-16.  This
	// code uses unsigned long and unsigned short to hold
	// codepoint values.  It requires unsigned long to be at least
	// 32 bits and unsigned short to be at least 16 bits, but it
	// will work fine if they are larger.
	unsigned long codepoint = 0L;
	for (unsigned int i = 2; i < len; i += 2)
	{
	    // Convert from UTF16-BE.  If we get a malformed
	    // codepoint, this code will generate incorrect output
	    // without giving a warning.  Specifically, a high
	    // codepoint not followed by a low codepoint will be
	    // discarded, and a low codepoint not preceded by a high
	    // codepoint will just get its low 10 bits output.
	    unsigned short bits =
		(((unsigned char) this->val[i]) << 8) +
		((unsigned char) this->val[i+1]);
	    if ((bits & 0xFC00) == 0xD800)
	    {
		codepoint = 0x10000 + ((bits & 0x3FF) << 10);
		continue;
	    }
	    else if ((bits & 0xFC00) == 0xDC00)
	    {
		if (codepoint != 0)
		{
		    QTC::TC("qpdf", "QPDF_String non-trivial UTF-16");
		}
		codepoint += bits & 0x3FF;
	    }
	    else
	    {
		codepoint = bits;
	    }

	    result += QUtil::toUTF8(codepoint);
	    codepoint = 0;
	}
    }
    else
    {
	for (unsigned int i = 0; i < len; ++i)
	{
	    result += QUtil::toUTF8((unsigned char) this->val[i]);
	}
    }
    return result;
}

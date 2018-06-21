#include <qpdf/QPDF_String.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.
#include <string.h>

// First element is 128
static unsigned short pdf_doc_to_unicode[] = {
    0x2022,    // 0x80    BULLET
    0x2020,    // 0x81    DAGGER
    0x2021,    // 0x82    DOUBLE DAGGER
    0x2026,    // 0x83    HORIZONTAL ELLIPSIS
    0x2014,    // 0x84    EM DASH
    0x2013,    // 0x85    EN DASH
    0x0192,    // 0x86    SMALL LETTER F WITH HOOK
    0x2044,    // 0x87    FRACTION SLASH (solidus)
    0x2039,    // 0x88    SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    0x203a,    // 0x89    SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    0x2212,    // 0x8a    MINUS SIGN
    0x2030,    // 0x8b    PER MILLE SIGN
    0x201e,    // 0x8c    DOUBLE LOW-9 QUOTATION MARK (quotedblbase)
    0x201c,    // 0x8d    LEFT DOUBLE QUOTATION MARK (double quote left)
    0x201d,    // 0x8e    RIGHT DOUBLE QUOTATION MARK (quotedblright)
    0x2018,    // 0x8f    LEFT SINGLE QUOTATION MARK (quoteleft)
    0x2019,    // 0x90    RIGHT SINGLE QUOTATION MARK (quoteright)
    0x201a,    // 0x91    SINGLE LOW-9 QUOTATION MARK (quotesinglbase)
    0x2122,    // 0x92    TRADE MARK SIGN
    0xfb01,    // 0x93    LATIN SMALL LIGATURE FI
    0xfb02,    // 0x94    LATIN SMALL LIGATURE FL
    0x0141,    // 0x95    LATIN CAPITAL LETTER L WITH STROKE
    0x0152,    // 0x96    LATIN CAPITAL LIGATURE OE
    0x0160,    // 0x97    LATIN CAPITAL LETTER S WITH CARON
    0x0178,    // 0x98    LATIN CAPITAL LETTER Y WITH DIAERESIS
    0x017d,    // 0x99    LATIN CAPITAL LETTER Z WITH CARON
    0x0131,    // 0x9a    LATIN SMALL LETTER DOTLESS I
    0x0142,    // 0x9b    LATIN SMALL LETTER L WITH STROKE
    0x0153,    // 0x9c    LATIN SMALL LIGATURE OE
    0x0161,    // 0x9d    LATIN SMALL LETTER S WITH CARON
    0x017e,    // 0x9e    LATIN SMALL LETTER Z WITH CARON
    0xfffd,    // 0x9f    UNDEFINED
    0x20ac,    // 0xa0    EURO SIGN
};

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

QPDF_String*
QPDF_String::new_utf16(std::string const& utf8_val)
{
    std::string result = "\xfe\xff";
    size_t len = utf8_val.length();
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char ch = static_cast<unsigned char>(utf8_val.at(i));
        if (ch < 128)
        {
            result += QUtil::toUTF16(ch);
        }
        else
        {
            size_t bytes_needed = 0;
            unsigned bit_check = 0x40;
            unsigned char to_clear = 0x80;
            while (ch & bit_check)
            {
                ++bytes_needed;
                to_clear |= bit_check;
                bit_check >>= 1;
            }

            if (((bytes_needed > 5) || (bytes_needed < 1)) ||
                ((i + bytes_needed) >= len))
            {
                result += "\xff\xfd";
            }
            else
            {
                unsigned long codepoint = (ch & ~to_clear);
                while (bytes_needed > 0)
                {
                    --bytes_needed;
                    ch = utf8_val.at(++i);
                    if ((ch & 0xc0) != 0x80)
                    {
                        --i;
                        codepoint = 0xfffd;
                        break;
                    }
                    codepoint <<= 6;
                    codepoint += (ch & 0x3f);
                }
                result += QUtil::toUTF16(codepoint);
            }
        }
    }
    return new QPDF_String(result);
}

std::string
QPDF_String::unparse()
{
    return unparse(false);
}

QPDFObject::object_type_e
QPDF_String::getTypeCode() const
{
    return QPDFObject::ot_string;
}

char const*
QPDF_String::getTypeName() const
{
    return "string";
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
	    char ch = this->val.at(i);
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
	result += "<" + QUtil::hex_encode(this->val) + ">";
    }
    else
    {
	result += "(";
	for (unsigned int i = 0; i < this->val.length(); ++i)
	{
	    char ch = this->val.at(i);
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
		    result += this->val.at(i);
		}
		else
		{
		    result += "\\" + QUtil::int_to_string_base(
                        static_cast<int>(static_cast<unsigned char>(ch)),
                        8, 3);
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
	(this->val.at(0) == '\xfe') && (this->val.at(1) == '\xff'))
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
		(static_cast<unsigned char>(this->val.at(i)) << 8) +
		static_cast<unsigned char>(this->val.at(i+1));
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
            unsigned char ch = static_cast<unsigned char>(this->val.at(i));
            unsigned short val = ch;
            if ((ch >= 128) && (ch <= 160))
            {
                val = pdf_doc_to_unicode[ch - 128];
            }
	    result += QUtil::toUTF8(val);
	}
    }
    return result;
}

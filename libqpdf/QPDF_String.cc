#include <qpdf/QPDF_String.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.
#include <string.h>

// See above about ctype.
static bool is_ascii_printable(char ch)
{
    return ((ch >= 32) && (ch <= 126));
}
static bool is_iso_latin1_printable(char ch)
{
    return (((ch >= 32) && (ch <= 126)) ||
            (static_cast<unsigned char>(ch) >= 160));
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
    std::string result;
    if (! QUtil::utf8_to_pdf_doc(utf8_val, result, '?'))
    {
        result = QUtil::utf8_to_utf16(utf8_val);
    }
    return new QPDF_String(result);
}

std::string
QPDF_String::unparse()
{
    return unparse(false);
}

JSON
QPDF_String::getJSON()
{
    return JSON::makeString(getUTF8Val());
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
    if (QUtil::is_utf16(this->val))
    {
        return QUtil::utf16_to_utf8(this->val);
    }
    else if ((val.length() >= 3) &&
             (val[0] == '\xEF') &&
             (val[1] == '\xBB') &&
             (val[2] == '\xBF'))
    {
        // PDF 2.0 allows UTF-8 strings when explicitly prefixed with
        // the above bytes, which is just UTF-8 encoding of U+FEFF.
        return this->val.substr(3);
    }
    else
    {
        return QUtil::pdf_doc_to_utf8(this->val);
    }
}

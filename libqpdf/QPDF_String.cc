#include <qpdf/QPDF_String.hh>

#include <qpdf/QUtil.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.
#include <string.h>

// See above about ctype.
static bool
is_ascii_printable(char ch)
{
    return ((ch >= 32) && (ch <= 126));
}
static bool
is_iso_latin1_printable(char ch)
{
    return (
        ((ch >= 32) && (ch <= 126)) || (static_cast<unsigned char>(ch) >= 160));
}

QPDF_String::QPDF_String(std::string const& val) :
    QPDFValue(::ot_string, "string"),
    val(val)
{
}

std::shared_ptr<QPDFValueProxy>
QPDF_String::create(std::string const& val)
{
    return do_create(new QPDF_String(val));
}

std::shared_ptr<QPDFValueProxy>
QPDF_String::create_utf16(std::string const& utf8_val)
{
    std::string result;
    if (!QUtil::utf8_to_pdf_doc(utf8_val, result, '?')) {
        result = QUtil::utf8_to_utf16(utf8_val);
    }
    return do_create(new QPDF_String(result));
}

std::shared_ptr<QPDFValueProxy>
QPDF_String::shallowCopy()
{
    return create(val);
}

std::string
QPDF_String::unparse()
{
    return unparse(false);
}

JSON
QPDF_String::getJSON(int json_version)
{
    if (json_version == 1) {
        return JSON::makeString(getUTF8Val());
    }
    // See if we can unambiguously represent as Unicode.
    bool is_unicode = false;
    std::string result;
    std::string candidate = getUTF8Val();
    if (QUtil::is_utf16(this->val) || QUtil::is_explicit_utf8(this->val)) {
        is_unicode = true;
        result = candidate;
    } else if (!useHexString()) {
        std::string test;
        if (QUtil::utf8_to_pdf_doc(candidate, test, '?') &&
            (test == this->val)) {
            // This is a PDF-doc string that can be losslessly encoded
            // as Unicode.
            is_unicode = true;
            result = candidate;
        }
    }
    if (is_unicode) {
        result = "u:" + result;
    } else {
        result = "b:" + QUtil::hex_encode(this->val);
    }
    return JSON::makeString(result);
}

bool
QPDF_String::useHexString() const
{
    // Heuristic: use the hexadecimal representation of a string if
    // there are any non-printable (in PDF Doc encoding) characters or
    // if too large of a proportion of the string consists of
    // non-ASCII characters.
    bool nonprintable = false;
    unsigned int non_ascii = 0;
    for (unsigned int i = 0; i < this->val.length(); ++i) {
        char ch = this->val.at(i);
        if ((ch == 0) ||
            (!(is_ascii_printable(ch) || strchr("\n\r\t\b\f", ch)))) {
            if ((ch >= 0) && (ch < 24)) {
                nonprintable = true;
            }
            ++non_ascii;
        }
    }
    return (nonprintable || (5 * non_ascii > val.length()));
}

std::string
QPDF_String::unparse(bool force_binary)
{
    bool use_hexstring = force_binary || useHexString();
    std::string result;
    if (use_hexstring) {
        result += "<" + QUtil::hex_encode(this->val) + ">";
    } else {
        result += "(";
        for (unsigned int i = 0; i < this->val.length(); ++i) {
            char ch = this->val.at(i);
            switch (ch) {
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
                if (is_iso_latin1_printable(ch)) {
                    result += this->val.at(i);
                } else {
                    result +=
                        "\\" +
                        QUtil::int_to_string_base(
                            static_cast<int>(static_cast<unsigned char>(ch)),
                            8,
                            3);
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
    if (QUtil::is_utf16(this->val)) {
        return QUtil::utf16_to_utf8(this->val);
    } else if (QUtil::is_explicit_utf8(this->val)) {
        // PDF 2.0 allows UTF-8 strings when explicitly prefixed with
        // the three-byte representation of U+FEFF.
        return this->val.substr(3);
    } else {
        return QUtil::pdf_doc_to_utf8(this->val);
    }
}

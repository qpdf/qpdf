#include <qpdf/QPDF_Name.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QUtil.hh>

QPDF_Name::QPDF_Name(std::string const& name) :
    QPDFValue(::ot_name, "name"),
    name(name)
{
}

std::shared_ptr<QPDFObject>
QPDF_Name::create(std::string const& name)
{
    return do_create(new QPDF_Name(name));
}

std::shared_ptr<QPDFObject>
QPDF_Name::copy(bool shallow)
{
    return create(name);
}

std::string
QPDF_Name::normalizeName(std::string const& name)
{
    if (name.empty()) {
        return name;
    }
    std::string result;
    result += name.at(0);
    for (size_t i = 1; i < name.length(); ++i) {
        char ch = name.at(i);
        // Don't use locale/ctype here; follow PDF spec guidelines.
        if (ch == '\0') {
            // QPDFTokenizer embeds a null character to encode an invalid #.
            result += "#";
        } else if (
            ch < 33 || ch == '#' || ch == '/' || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
            ch == '<' || ch == '>' || ch == '[' || ch == ']' || ch == '%' || ch > 126) {
            result += QUtil::hex_encode_char(ch);
        } else {
            result += ch;
        }
    }
    return result;
}

std::string
QPDF_Name::unparse()
{
    return normalizeName(this->name);
}

JSON
QPDF_Name::getJSON(int json_version)
{
    if (json_version == 1) {
        return JSON::makeString(normalizeName(this->name));
    } else {
        bool has_8bit_chars;
        bool is_valid_utf8;
        bool is_utf16;
        QUtil::analyze_encoding(this->name, has_8bit_chars, is_valid_utf8, is_utf16);
        if (!has_8bit_chars || is_valid_utf8) {
            return JSON::makeString(this->name);
        } else {
            return JSON::makeString("n:" + normalizeName(this->name));
        }
    }
}

void
QPDF_Name::writeJSON(int json_version, JSON::Writer& p)
{
    if (json_version == 1) {
        p << "\"" << JSON::Writer::encode_string(normalizeName(name)) << "\"";
    } else {
        bool has_8bit_chars;
        bool is_valid_utf8;
        bool is_utf16;
        QUtil::analyze_encoding(this->name, has_8bit_chars, is_valid_utf8, is_utf16);
        if (!has_8bit_chars || is_valid_utf8) {
            p << "\"" << JSON::Writer::encode_string(name) << "\"";
        } else {
            p << "\"n:" << JSON::Writer::encode_string(normalizeName(name)) << "\"";
        }
    }
}

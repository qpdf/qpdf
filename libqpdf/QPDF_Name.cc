#include <qpdf/QPDF_Name.hh>

#include <qpdf/QUtil.hh>
#include <stdio.h>
#include <string.h>

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
            // QPDFTokenizer embeds a null character to encode an
            // invalid #.
            result += "#";
        } else if (
            ch < 33 || ch == '#' || ch == '/' || ch == '(' || ch == ')' ||
            ch == '{' || ch == '}' || ch == '<' || ch == '>' || ch == '[' ||
            ch == ']' || ch == '%' || ch > 126) {
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
        return JSON::makeString(this->name);
    }
}

#include <qpdf/QPDF_Name.hh>

#include <string.h>
#include <stdio.h>
#include <qpdf/QUtil.hh>

QPDF_Name::QPDF_Name(std::string const& name) :
    name(name)
{
}

QPDF_Name::~QPDF_Name()
{
}

std::string
QPDF_Name::normalizeName(std::string const& name)
{
    if (name.empty())
    {
        return name;
    }
    std::string result;
    result += name.at(0);
    for (size_t i = 1; i < name.length(); ++i)
    {
        char ch = name.at(i);
        // Don't use locale/ctype here; follow PDF spec guidelines.
        if (ch == '\0')
        {
            // QPDFTokenizer embeds a null character to encode an
            // invalid #.
            result += "#";
        }
        else if (strchr("#()<>[]{}/%", ch) || (ch < 33) || (ch > 126))
        {
            result += "#" + QUtil::hex_encode(std::string(&ch, 1));
        }
        else
        {
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
QPDF_Name::getJSON()
{
    return JSON::makeString(normalizeName(this->name));
}

QPDFObject::object_type_e
QPDF_Name::getTypeCode() const
{
    return QPDFObject::ot_name;
}

char const*
QPDF_Name::getTypeName() const
{
    return "name";
}

std::string
QPDF_Name::getName() const
{
    return this->name;
}

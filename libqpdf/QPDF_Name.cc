
#include <qpdf/QPDF_Name.hh>

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
    std::string result;
    char num[4];
    result += name[0];
    for (unsigned int i = 1; i < name.length(); ++i)
    {
	char ch = name[i];
	// Don't use locale/ctype here; follow PDF spec guidlines.
	if (strchr("#()<>[]{}/%", ch) || (ch < 33) || (ch > 126))
	{
	    sprintf(num, "#%02x", (unsigned char) ch);
	    result += num;
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

std::string
QPDF_Name::getName() const
{
    return this->name;
}

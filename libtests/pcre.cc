#include <qpdf/PCRE.hh>
#include <iostream>
#include <string.h>

int main(int argc, char* argv[])
{
    if ((argc == 2) && (strcmp(argv[1], "--unicode-classes-supported") == 0))
    {
	try
	{
	    PCRE("^([\\p{L}]+)", PCRE_UTF8);
	    std::cout << "1" << std::endl;
	}
	catch (std::exception&)
	{
	    std::cout << "0" << std::endl;
	}
	return 0;
    }

    if ((argc == 2) && (strcmp(argv[1], "--unicode-classes") == 0))
    {
	PCRE::test(1);
    }
    else
    {
	PCRE::test();
    }
    return 0;
}

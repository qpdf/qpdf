#include <qpdf/Pl_ASCIIHexDecoder.hh>

#include <qpdf/Pl_StdioFile.hh>
#include <iostream>
#include <stdlib.h>

int main()
{
    Pl_StdioFile out("stdout", stdout);
    Pl_ASCIIHexDecoder decode("decode", &out);

    try
    {
	unsigned char buf[10000];
	bool done = false;
	while (! done)
	{
	    size_t len = fread(buf, 1, sizeof(buf), stdin);
	    if (len <= 0)
	    {
		done = true;
	    }
	    else
	    {
		decode.write(buf, len);
	    }
	}
	decode.finish();
    }
    catch (std::exception& e)
    {
	std::cerr << e.what() << std::endl;
	 exit(2);
    }

    return 0;
}

#include <qpdf/Pl_LZWDecoder.hh>

#include <qpdf/Pl_StdioFile.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
    bool early_code_change = true;
    if ((argc == 2) && (strcmp(argv[1], "--no-early-code-change") == 0))
    {
	early_code_change = false;
    }

    Pl_StdioFile out("stdout", stdout);
    Pl_LZWDecoder decode("decode", &out, early_code_change);

    try
    {
	unsigned char buf[10000];
	bool done = false;
	while (! done)
	{
	    int len = read(0, buf, sizeof(buf));
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

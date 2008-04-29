#include <qpdf/Pl_ASCII85Decoder.hh>

#include <qpdf/Pl_StdioFile.hh>
#include <iostream>

int main()
{
    Pl_StdioFile out("stdout", stdout);
    Pl_ASCII85Decoder decode("decode", &out);

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

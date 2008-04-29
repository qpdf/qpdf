#include <qpdf/Pl_LZWDecoder.hh>

#include <qpdf/Pl_StdioFile.hh>
#include <iostream>

int main()
{
    Pl_StdioFile out("stdout", stdout);
    // We don't exercise LZWDecoder with early code change false
    // because we have no way to generate such an LZW stream.
    Pl_LZWDecoder decode("decode", &out, true);

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

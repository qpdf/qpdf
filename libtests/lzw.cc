#include <qpdf/Pl_LZWDecoder.hh>

#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
    bool early_code_change = true;
    if ((argc == 4) && (strcmp(argv[3], "--no-early-code-change") == 0))
    {
	early_code_change = false;
    }

    if (argc < 3)
    {
	std::cerr << "Usage: lzw infile outfile [ --no-early-code-change ]"
		  << std::endl;
	exit(2);
    }

    try
    {
	char* infilename = argv[1];
	char* outfilename = argv[2];

	FILE* infile = QUtil::safe_fopen(infilename, "rb");
	FILE* outfile = QUtil::safe_fopen(outfilename, "wb");

	Pl_StdioFile out("output", outfile);
	Pl_LZWDecoder decode("decode", &out, early_code_change);

	unsigned char buf[10000];
	bool done = false;
	while (! done)
	{
	    size_t len = fread(buf, 1, sizeof(buf), infile);
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

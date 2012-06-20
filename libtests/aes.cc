#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_StdioFile.hh>

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>

static void usage()
{
    std::cerr << "Usage: aes [+-]cbc { -encrypt | -decrypt }"
	      << " hex-key infile outfile" << std::endl;
    exit(2);
}

int main(int argc, char* argv[])
{
    if (argc != 6)
    {
	usage();
   }

    char* cbc = argv[1];
    char* action = argv[2];
    char* hexkey = argv[3];
    char* infilename = argv[4];
    char* outfilename = argv[5];

    bool cbc_mode = true;
    if (strcmp(cbc, "-cbc") == 0)
    {
	cbc_mode = false;
    }
    else if (strcmp(cbc, "+cbc") != 0)
    {
	usage();
    }

    bool encrypt = true;
    if (strcmp(action, "-decrypt") == 0)
    {
	encrypt = false;
    }
    else if (strcmp(action, "-encrypt") != 0)
    {
	usage();
    }

    unsigned int hexkeylen = (unsigned int)strlen(hexkey);
    unsigned int keylen = hexkeylen / 2;
    if (keylen != Pl_AES_PDF::key_size)
    {
	std::cerr << "key length must be " << Pl_AES_PDF::key_size
		  << " bytes" << std::endl;
	exit(2);
    }

    FILE* infile = fopen(infilename, "rb");
    if (infile == 0)
    {
	std::cerr << "can't open " << infilename << std::endl;
	exit(2);
    }

    FILE* outfile = fopen(outfilename, "wb");
    if (outfile == 0)
    {
	std::cerr << "can't open " << outfilename << std::endl;
	exit(2);
    }

    unsigned char key[Pl_AES_PDF::key_size];
    for (unsigned int i = 0; i < strlen(hexkey); i += 2)
    {
	char t[3];
	t[0] = hexkey[i];
	t[1] = hexkey[i + 1];
	t[2] = '\0';

	long val = strtol(t, 0, 16);
	key[i/2] = (unsigned char) val;
    }

    Pl_StdioFile* out = new Pl_StdioFile("stdout", outfile);
    Pl_AES_PDF* aes = new Pl_AES_PDF("aes_128_cbc", out, encrypt, key);
    if (! cbc_mode)
    {
	aes->disableCBC();
    }

    // 16 < buffer size, buffer_size is not a multiple of 8 for testing
    unsigned char buf[83];
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
	    aes->write(buf, len);
	}
    }
    aes->finish();
    delete aes;
    delete out;
    fclose(infile);
    fclose(outfile);
    return 0;
}

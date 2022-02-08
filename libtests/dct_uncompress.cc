#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: dct_uncompress infile outfile"
                  << std::endl;
        exit(2);
    }

    char* infilename = argv[1];
    char* outfilename = argv[2];

    FILE* infile = QUtil::safe_fopen(infilename, "rb");
    FILE* outfile = QUtil::safe_fopen(outfilename, "wb");
    Pl_StdioFile out("stdout", outfile);
    unsigned char buf[100];
    bool done = false;
    Pl_DCT dct("dct", &out);
    while (! done)
    {
        size_t len = fread(buf, 1, sizeof(buf), infile);
        if (len <= 0)
        {
            done = true;
        }
        else
        {
            dct.write(buf, len);
        }
    }
    dct.finish();
    fclose(infile);
    fclose(outfile);
    return 0;
}

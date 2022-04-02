#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: runlength {-encode|-decode} infile outfile"
                  << std::endl;
        exit(2);
    }

    bool encode = (strcmp("-encode", argv[1]) == 0);
    char* infilename = argv[2];
    char* outfilename = argv[3];

    FILE* infile = QUtil::safe_fopen(infilename, "rb");
    FILE* outfile = QUtil::safe_fopen(outfilename, "wb");
    Pl_StdioFile out("stdout", outfile);
    unsigned char buf[100];
    bool done = false;
    Pl_RunLength rl(
        "runlength",
        &out,
        (encode ? Pl_RunLength::a_encode : Pl_RunLength::a_decode));
    while (!done) {
        size_t len = fread(buf, 1, sizeof(buf), infile);
        if (len <= 0) {
            done = true;
        } else {
            rl.write(buf, len);
        }
    }
    rl.finish();
    fclose(infile);
    fclose(outfile);
    return 0;
}

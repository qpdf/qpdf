#include <qpdf/assert_test.h>

#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static void
usage()
{
    std::cerr << "Usage: dct_compress infile outfile width height {rgb|cmyk|gray}" << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    if (argc != 6) {
        usage();
    }

    char* infilename = argv[1];
    char* outfilename = argv[2];
    JDIMENSION width = QUtil::string_to_uint(argv[3]);
    JDIMENSION height = QUtil::string_to_uint(argv[4]);
    char* colorspace = argv[5];
    J_COLOR_SPACE cs =
        ((strcmp(colorspace, "rgb") == 0)        ? JCS_RGB
             : (strcmp(colorspace, "cmyk") == 0) ? JCS_CMYK
             : (strcmp(colorspace, "gray") == 0) ? JCS_GRAYSCALE
                                                 : JCS_UNKNOWN);
    int components = 0;
    switch (cs) {
    case JCS_RGB:
        components = 3;
        break;
    case JCS_CMYK:
        components = 4;
        break;
    case JCS_GRAYSCALE:
        components = 1;
        break;
    default:
        usage();
        break;
    }

    FILE* infile = QUtil::safe_fopen(infilename, "rb");
    FILE* outfile = QUtil::safe_fopen(outfilename, "wb");
    Pl_StdioFile out("stdout", outfile);
    bool called = false;
    auto callback = [&called](jpeg_compress_struct*) { called = true; };
    auto config = Pl_DCT::make_compress_config(callback);
    unsigned char buf[100];
    Pl_DCT dct("dct", &out, width, height, components, cs, config.get());
    while (size_t len = fread(buf, 1, sizeof(buf), infile)) {
        dct.write(buf, len);
    }
    dct.finish();
    assert(called);

    fclose(infile);
    fclose(outfile);
    return 0;
}

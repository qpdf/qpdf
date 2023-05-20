#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void
usage()
{
    std::cerr << "Usage: dct_compress infile outfile width height"
              << " {rgb|cmyk|gray}" << std::endl;
    exit(2);
}

class Callback: public Pl_DCT::CompressConfig
{
  public:
    Callback() :
        called(false)
    {
    }
    ~Callback() override = default;
    void apply(jpeg_compress_struct*) override;
    bool called;
};

void
Callback::apply(jpeg_compress_struct*)
{
    this->called = true;
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
    unsigned char buf[100];
    bool done = false;
    Callback callback;
    Pl_DCT dct("dct", &out, width, height, components, cs, &callback);
    while (!done) {
        size_t len = fread(buf, 1, sizeof(buf), infile);
        if (len <= 0) {
            done = true;
        } else {
            dct.write(buf, len);
        }
    }
    dct.finish();
    if (!callback.called) {
        std::cout << "Callback was not called" << std::endl;
    }
    fclose(infile);
    fclose(outfile);
    return 0;
}

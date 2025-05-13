#include <qpdf/assert_test.h>

#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>

#include <cstdlib>
#include <cstring>
#include <iostream>

void
run(char const* filename,
    char const* filter,
    bool encode,
    unsigned int columns,
    unsigned int bits_per_sample,
    unsigned int samples_per_pixel)
{
    FILE* in = QUtil::safe_fopen(filename, "rb");
    FILE* o1 = QUtil::safe_fopen("out", "wb");
    Pipeline* out = new Pl_StdioFile("out", o1);
    Pipeline* pl = nullptr;
    if (strcmp(filter, "png") == 0) {
        pl = new Pl_PNGFilter(
            "png",
            out,
            encode ? Pl_PNGFilter::a_encode : Pl_PNGFilter::a_decode,
            columns,
            samples_per_pixel,
            bits_per_sample);
    } else if (strcmp(filter, "tiff") == 0) {
        pl = new Pl_TIFFPredictor(
            "png",
            out,
            encode ? Pl_TIFFPredictor::a_encode : Pl_TIFFPredictor::a_decode,
            columns,
            samples_per_pixel,
            bits_per_sample);
    } else {
        std::cerr << "unknown filter " << filter << '\n';
        exit(2);
    }
    assert((2 * (columns + 1)) < 1024);
    unsigned char buf[1024];
    size_t len;
    while (true) {
        len = fread(buf, 1, (2 * columns) + 1, in);
        if (len == 0) {
            break;
        }
        pl->write(buf, len);
        len = fread(buf, 1, 1, in);
        if (len == 0) {
            break;
        }
        pl->write(buf, len);
        len = fread(buf, 1, 1, in);
        if (len == 0) {
            break;
        }
        pl->write(buf, len);
    }

    pl->finish();
    delete pl;
    delete out;
    fclose(o1);
    fclose(in);

    std::cout << "done" << '\n';
}

int
main(int argc, char* argv[])
{
    if (argc != 7) {
        std::cerr << "Usage: predictor {png|tiff} {en,de}code filename"
                  << " columns samples-per-pixel bits-per-sample" << '\n';
        exit(2);
    }
    char* filter = argv[1];
    bool encode = (strcmp(argv[2], "encode") == 0);
    char* filename = argv[3];
    int columns = QUtil::string_to_int(argv[4]);
    int samples_per_pixel = QUtil::string_to_int(argv[5]);
    int bits_per_sample = QUtil::string_to_int(argv[6]);

    try {
        run(filename,
            filter,
            encode,
            QIntC::to_uint(columns),
            QIntC::to_uint(bits_per_sample),
            QIntC::to_uint(samples_per_pixel));
    } catch (std::exception& e) {
        std::cout << e.what() << '\n';
    }
    return 0;
}

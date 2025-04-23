#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " [ -skip=N ] { -uncompress | -compress[=n] }" << std::endl
              << "  skip=N will discard first N bytes of input before doing [un]compress" << std::endl
              << "If n is specified with -compress, it is a zlib compression level from"
              << std::endl
              << "1 to 9 where lower numbers are faster and less compressed and higher" << std::endl
              << "numbers are slower and more compressed" << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << whoami << " from qpdf version " << QPDF::QPDFVersion() << std::endl;
        exit(0);
    }

    Pl_Flate::action_e action = Pl_Flate::a_inflate;

    bool args_valid = false;
    size_t skip = 0;
    for (int i = 1; i < argc; ++i) {
        if ((strncmp(argv[i], "-skip=", 6) == 0)) {
            skip = QUtil::string_to_int(argv[i] + 6);
        } else if ((strcmp(argv[i], "-uncompress") == 0)) {
             args_valid = true;
        } else if ((strcmp(argv[i], "-compress") == 0)) {
            action = Pl_Flate::a_deflate;
            args_valid = true;
        } else if ((strncmp(argv[i], "-compress=", 10) == 0)) {
            action = Pl_Flate::a_deflate;
            int level = QUtil::string_to_int(argv[i] + 10);
            Pl_Flate::setCompressionLevel(level);
            args_valid = true;
        } else if (strcmp(argv[i], "--_zopfli") == 0) {
            // Undocumented option, but that doesn't mean someone doesn't use it...
            // This is primarily here to support the test suite.
            std::cout << (Pl_Flate::zopfli_supported() ? "1" : "0")
                      << (Pl_Flate::zopfli_enabled() ? "1" : "0") << std::endl;
            return 0;
        } else {
            usage();
        }
    }

    if (!args_valid) {
        usage();
    }

    bool warn = false;
    try {
        QUtil::binary_stdout();
        QUtil::binary_stdin();
        Pl_Flate::zopfli_check_env();
        auto out = std::make_shared<Pl_StdioFile>("stdout", stdout);
        auto flate = std::make_shared<Pl_Flate>("flate", out.get(), action);
        flate->setWarnCallback([&warn](char const* msg, int code) {
            warn = true;
            std::cerr << whoami << ": WARNING: zlib code " << code << ", msg = " << msg
                      << std::endl;
        });

        unsigned char buf[10000];
        bool done = false;

        while (skip > 0) {
            size_t chunk = skip > 10000 ? 10000 : skip;
            size_t len = fread(buf, 1, chunk, stdin);
            if (len >= 0 ) {
                skip -= len;
            } else {
                break;
            }
        }

        while (!done) {
            size_t len = fread(buf, 1, sizeof(buf), stdin);
            if (len <= 0) {
                done = true;
            } else {
                flate->write(buf, len);
            }
        }
        flate->finish();
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << std::endl;
        exit(2);
    }

    if (warn) {
        exit(3);
    }
    return 0;
}

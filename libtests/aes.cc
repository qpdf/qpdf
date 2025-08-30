#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static void
usage()
{
    std::cerr << "Usage: aes options hex-key infile outfile" << '\n'
              << "  -cbc         -- disable CBC mode" << '\n'
              << "  +cbc         -- enable CBC mode" << '\n'
              << "  -encrypt     -- encrypt" << '\n'
              << "  -decrypt     -- decrypt CBC mode" << '\n'
              << "  -zero-iv     -- use zero initialization vector" << '\n'
              << "  -static-iv   -- use static initialization vector" << '\n'
              << "  -no-padding  -- disable padding" << '\n'
              << "Options must precede key and file names." << '\n';
    exit(2);
}

int
main(int argc, char* argv[])
{
    bool encrypt = true;
    bool cbc_mode = true;
    char* hexkey = nullptr;
    char* infilename = nullptr;
    char* outfilename = nullptr;
    bool zero_iv = false;
    bool static_iv = false;
    bool disable_padding = false;

    for (int i = 1; i < argc; ++i) {
        char* arg = argv[i];
        if ((arg[0] == '-') || (arg[0] == '+')) {
            if (strcmp(arg, "-cbc") == 0) {
                cbc_mode = false;
            } else if (strcmp(arg, "+cbc") == 0) {
                cbc_mode = true;
            } else if (strcmp(arg, "-decrypt") == 0) {
                encrypt = false;
            } else if (strcmp(arg, "-encrypt") == 0) {
                encrypt = true;
            } else if (strcmp(arg, "-zero-iv") == 0) {
                zero_iv = true;
            } else if (strcmp(arg, "-static-iv") == 0) {
                static_iv = true;
            } else if (strcmp(arg, "-no-padding") == 0) {
                disable_padding = true;
            } else {
                usage();
            }
        } else if (argc == i + 3) {
            hexkey = argv[i];
            infilename = argv[i + 1];
            outfilename = argv[i + 2];
            break;
        } else {
            usage();
        }
    }
    if (outfilename == nullptr) {
        usage();
    }

    unsigned int hexkeylen = QIntC::to_uint(strlen(hexkey));
    unsigned int keylen = hexkeylen / 2;

    FILE* infile = QUtil::safe_fopen(infilename, "rb");
    FILE* outfile = QUtil::safe_fopen(outfilename, "wb");
    auto* key = new unsigned char[keylen];
    for (unsigned int i = 0; i < strlen(hexkey); i += 2) {
        char t[3];
        t[0] = hexkey[i];
        t[1] = hexkey[i + 1];
        t[2] = '\0';

        long val = strtol(t, nullptr, 16);
        key[i / 2] = static_cast<unsigned char>(val);
    }

    std::string keystr(reinterpret_cast<char const*>(key), keylen);
    auto* out = new Pl_StdioFile("stdout", outfile);
    auto* aes = new Pl_AES_PDF("aes_128_cbc", out, encrypt, keystr);
    delete[] key;
    key = nullptr;
    if (!cbc_mode) {
        aes->disableCBC();
    }
    if (zero_iv) {
        aes->useZeroIV();
    } else if (static_iv) {
        aes->useStaticIV();
    }
    if (disable_padding) {
        aes->disablePadding();
    }

    // 16 < buffer size, buffer_size is not a multiple of 8 for testing
    unsigned char buf[83];
    std::string data;
    while (true) {
        size_t len = fread(buf, 1, sizeof(buf), infile);
        data.append(reinterpret_cast<char const*>(buf), len);
        if (len <= 0) {
            break;
        }
    }
    aes->write(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    aes->finish();
    delete aes;
    delete out;
    fclose(infile);
    fclose(outfile);
    return 0;
}

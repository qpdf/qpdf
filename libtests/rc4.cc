#include <qpdf/assert_test.h>

#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static void
other_tests()
{
    // Test cases not covered by the pipeline: string as key, convert
    // in place
    RC4 r(reinterpret_cast<unsigned char const*>("quack"));
    auto data = std::make_unique<unsigned char[]>(6);
    memcpy(data.get(), "potato", 6);
    r.process(data.get(), 6, data.get());
    assert(memcmp(data.get(), "\xa5\x6f\xe7\x27\x2b\x5c", 6) == 0);
    std::cout << "passed" << '\n';
}

int
main(int argc, char* argv[])
{
    if ((argc == 2) && (strcmp(argv[1], "other") == 0)) {
        other_tests();
        return 0;
    }

    if (argc != 4) {
        std::cerr << "Usage: rc4 hex-key infile outfile" << '\n';
        exit(2);
    }

    char* hexkey = argv[1];
    char* infilename = argv[2];
    char* outfilename = argv[3];
    unsigned int hexkeylen = QIntC::to_uint(strlen(hexkey));
    unsigned int keylen = hexkeylen / 2;
    auto* key = new unsigned char[keylen + 1];
    key[keylen] = '\0';

    FILE* infile = QUtil::safe_fopen(infilename, "rb");
    for (unsigned int i = 0; i < strlen(hexkey); i += 2) {
        char t[3];
        t[0] = hexkey[i];
        t[1] = hexkey[i + 1];
        t[2] = '\0';

        long val = strtol(t, nullptr, 16);
        key[i / 2] = static_cast<unsigned char>(val);
    }

    FILE* outfile = QUtil::safe_fopen(outfilename, "wb");
    auto* out = new Pl_StdioFile("stdout", outfile);
    // Use a small buffer size (64) for testing
    std::string keystr(reinterpret_cast<char const*>(key), keylen);
    auto* rc4 = new Pl_RC4("rc4", out, keystr, 64U);
    delete[] key;

    // 64 < buffer size < 512, buffer_size is not a power of 2 for testing
    unsigned char buf[100];
    bool done = false;
    while (!done) {
        size_t len = fread(buf, 1, sizeof(buf), infile);
        if (len <= 0) {
            done = true;
        } else {
            rc4->write(buf, len);
        }
    }
    rc4->finish();
    delete rc4;
    delete out;
    fclose(infile);
    fclose(outfile);
    return 0;
}

#include <qpdf/assert_test.h>

#include <qpdf/Pl_Base64.hh>

#include <qpdf/Pl_OStream.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

static bool
write_some(FILE* f, size_t bytes, Pipeline* p)
{
    unsigned char buf[1000];
    assert(bytes <= sizeof(buf));
    size_t len = fread(buf, 1, bytes, f);
    if (len > 0) {
        p->write(buf, len);
    }
    if (len < bytes) {
        if (ferror(f)) {
            std::cerr << "error reading file" << '\n';
            exit(2);
        }
        p->finish();
        return false;
    }
    return (len == bytes);
}

static void
usage()
{
    std::cerr << "Usage: base64 encode|decode" << '\n';
    exit(2);
}

int
main(int argc, char* argv[])
{
    if (argc != 2) {
        usage();
    }
    QUtil::binary_stdout();
    QUtil::binary_stdin();
    Pl_Base64::action_e action = Pl_Base64::a_decode;
    if (strcmp(argv[1], "encode") == 0) {
        action = Pl_Base64::a_encode;
    } else if (strcmp(argv[1], "decode") != 0) {
        usage();
    }

    try {
        Pl_OStream out("stdout", std::cout);
        Pl_Base64 decode("decode", &out, action);
        // The comments are "n: n%4 n%3", where n is the number of
        // bytes read at the end of the call, and are there to
        // indicate that we are reading in chunks that exercise
        // various boundary conditions around subsequent writes and
        // the state of buf and pos. There are some writes that don't
        // do flush at all, some that call flush multiple times, and
        // some that start in the middle and do flush, and this is
        // true for both encode and decode.
        if (write_some(stdin, 1, &decode) && //  1: 1 1
            write_some(stdin, 4, &decode) && //  5: 1 2
            write_some(stdin, 2, &decode) && //  7: 3 1
            write_some(stdin, 2, &decode) && //  9: 1 0
            write_some(stdin, 7, &decode) && // 16: 0 1
            write_some(stdin, 1, &decode) && // 17: 1 2
            write_some(stdin, 9, &decode) && // 26: 2 2
            write_some(stdin, 2, &decode)) { // 28: 0 1
            while (write_some(stdin, 1000, &decode)) {
            }
        }
    } catch (std::exception& e) {
        std::cout << "exception: " << e.what() << '\n';
        exit(2);
    }

    return 0;
}

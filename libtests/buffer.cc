#include <qpdf/assert_test.h>

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>

static unsigned char*
uc(char const* s)
{
    return QUtil::unsigned_char_pointer(s);
}

int
main()
{
    {
        // Test that buffers can be copied by value.
        Buffer bc1(2);
        unsigned char* bc1p = bc1.getBuffer();
        bc1p[0] = 'Q';
        bc1p[1] = 'W';
        Buffer bc2(bc1);
        bc1p[0] = 'R';
        unsigned char* bc2p = bc2.getBuffer();
        assert(bc2p != bc1p);
        assert(bc2p[0] == 'Q');
        assert(bc2p[1] == 'W');
        bc2 = bc1;
        bc2p = bc2.getBuffer();
        assert(bc2p != bc1p);
        assert(bc2p[0] == 'R');
        assert(bc2p[1] == 'W');
    }

    {
        // Test that buffers can be moved.
        Buffer bm1(2);
        unsigned char* bm1p = bm1.getBuffer();
        bm1p[0] = 'Q';
        bm1p[1] = 'W';
        Buffer bm2(std::move(bm1));
        bm1p[0] = 'R';
        unsigned char* bm2p = bm2.getBuffer();
        assert(bm2p == bm1p);
        assert(bm2p[0] == 'R');

        Buffer bm3 = std::move(bm2);
        unsigned char* bm3p = bm3.getBuffer();
        assert(bm3p == bm2p);
    }

    try {
        Pl_Discard discard;
        Pl_Count count("count", &discard);
        Pl_Buffer bp1("bp1", &count);
        bp1.write(uc("12345"), 5);
        bp1.write(uc("67890"), 5);
        bp1.finish();
        std::cout << "count: " << count.getCount() << std::endl;
        bp1.write(uc("abcde"), 5);
        bp1.write(uc("fghij"), 6);
        bp1.finish();
        std::cout << "count: " << count.getCount() << std::endl;
        Buffer* b = bp1.getBuffer();
        std::cout << "size: " << b->getSize() << std::endl;
        std::cout << "data: " << b->getBuffer() << std::endl;
        delete b;
        bp1.write(uc("qwert"), 5);
        bp1.write(uc("yuiop"), 6);
        bp1.finish();
        std::cout << "count: " << count.getCount() << std::endl;
        b = bp1.getBuffer();
        std::cout << "size: " << b->getSize() << std::endl;
        std::cout << "data: " << b->getBuffer() << std::endl;
        delete b;

        Pl_Buffer bp2("bp2");
        bp2.write(uc("moo"), 3);
        bp2.write(uc("quack"), 6);
        try {
            delete bp2.getBuffer();
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        bp2.finish();
        b = bp2.getBuffer();
        std::cout << "size: " << b->getSize() << std::endl;
        std::cout << "data: " << b->getBuffer() << std::endl;
        delete b;

        unsigned char lbuf[10];
        Buffer b1(lbuf, 10);
        if (!((b1.getBuffer() == lbuf) && (b1.getSize() == 10))) {
            throw std::logic_error("hand-created buffer is not as expected");
        }

        Pl_Buffer bp3("bp3");
        b = bp3.getBuffer();
        std::cout << "size: " << b->getSize() << std::endl;
        delete b;
        // Should be able to call getBuffer again and get an empty buffer
        b = bp3.getBuffer();
        std::cout << "size: " << b->getSize() << std::endl;
        delete b;
        // Also can write 0 and do it.
        bp3.write(uc(""), 0);
        bp3.finish();
        b = bp3.getBuffer();
        std::cout << "size: " << b->getSize() << std::endl;
        delete b;

        // Malloc buffer should behave similarly.
        Pl_Buffer bp4("bp4");
        bp4.write(uc("asdf"), 4);
        unsigned char* mbuf;
        size_t len;
        try {
            bp4.getMallocBuffer(&mbuf, &len);
            assert(false);
        } catch (std::logic_error& e) {
            std::cout << "malloc buffer logic error: " << e.what() << std::endl;
        }
        bp4.finish();
        bp4.getMallocBuffer(&mbuf, &len);
        assert(len == 4);
        assert(memcmp(mbuf, uc("asdf"), 4) == 0);
        free(mbuf);
        bp4.write(uc(""), 0);
        bp4.finish();
        bp4.getMallocBuffer(&mbuf, &len);
        assert(mbuf == nullptr);
        assert(len == 0);
    } catch (std::exception& e) {
        std::cout << "unexpected exception: " << e.what() << std::endl;
        exit(2);
    }

    std::cout << "done" << std::endl;
    return 0;
}

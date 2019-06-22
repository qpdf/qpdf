#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QUtil.hh>
#include <stdlib.h>
#include <stdexcept>
#include <iostream>
#include <cassert>

static unsigned char* uc(char const* s)
{
    return QUtil::unsigned_char_pointer(s);
}

int main()
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

    try
    {
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
	try
	{
	    delete bp2.getBuffer();
	}
	catch (std::exception& e)
	{
	    std::cout << e.what() << std::endl;
	}
	bp2.finish();
	b = bp2.getBuffer();
	std::cout << "size: " << b->getSize() << std::endl;
	std::cout << "data: " << b->getBuffer() << std::endl;
	delete b;

	unsigned char lbuf[10];
	Buffer b1(lbuf, 10);
	if (! ((b1.getBuffer() == lbuf) &&
	       (b1.getSize() == 10)))
	{
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
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
	exit(2);
    }

    std::cout << "done" << std::endl;
    return 0;
}

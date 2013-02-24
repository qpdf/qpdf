#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QUtil.hh>
#include <stdlib.h>
#include <stdexcept>
#include <iostream>

static unsigned char* uc(char const* s)
{
    return QUtil::unsigned_char_pointer(s);
}

int main()
{
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
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
	exit(2);
    }

    std::cout << "done" << std::endl;
    return 0;
}

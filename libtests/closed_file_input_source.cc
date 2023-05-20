#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/FileInputSource.hh>

#include <cstdio>
#include <iostream>

void
check(std::string const& what, bool result)
{
    if (!result) {
        std::cout << "FAIL: " << what << std::endl;
    }
}

void
do_tests(InputSource* is)
{
    check("get name", "input" == is->getName());
    check("initial tell", 0 == is->tell());
    is->seek(11, SEEK_SET);
    check("tell after SEEK_SET", 11 == is->tell());
    check("read offset 11", "Offset 11" == is->readLine(100));
    check("last offset after read 11", 11 == is->getLastOffset());
    check("tell after read", 21 == is->tell());
    is->findAndSkipNextEOL();
    check("tell after findAndSkipNextEOL", 522 == is->tell());
    char b[1];
    b[0] = '\0';
#ifdef _WIN32
    // Empirical evidence, and the passage of the rest of the qpdf
    // test suite, suggest that this is working on Windows in the way
    // that it needs to work. If this ifdef is made to be true on
    // Windows, it passes with ClosedFileInputSource but not with
    // FileInputSource, which doesn't make any sense since
    // ClosedFileInputSource is calling FileInputSource to do its
    // work.
    is->seek(521, SEEK_SET);
    is->read(b, 1);
#else
    is->unreadCh('\n');
    check("read unread character", 1 == is->read(b, 1));
    check("got character", '\n' == b[0]);
#endif
    check("last offset after read unread", 521 == is->getLastOffset());
    is->seek(0, SEEK_END);
    check("tell at end", 556 == is->tell());
    is->seek(-25, SEEK_END);
    check("tell before end", 531 == is->tell());
    check("last offset unchanged after seek", 521 == is->getLastOffset());
    is->seek(-9, SEEK_CUR);
    check("tell after SEEK_CUR", 522 == is->tell());
    check("read offset 522", "9 before" == is->readLine(100));
    check("last offset after read", 522 == is->getLastOffset());
    is->rewind();
    check("last offset unchanged after rewind", 522 == is->getLastOffset());
    check("tell after rewind", 0 == is->tell());
    check("read offset at beginning", "!00000000?" == is->readLine(100));
    check("last offset after read 0", 0 == is->getLastOffset());
}

int
main()
{
    // This test is designed to work with a specified input file.
    std::cout << "testing with ClosedFileInputSource\n";
    ClosedFileInputSource cf("input");
    do_tests(&cf);
    std::cout << "testing with ClosedFileInputSource in stay open mode\n";
    ClosedFileInputSource cf2("input");
    cf2.stayOpen(true);
    do_tests(&cf2);
    cf2.stayOpen(false);
    std::cout << "testing with FileInputSource\n";
    FileInputSource f("input");
    do_tests(&f);
    std::cout << "all assertions passed" << std::endl;
    return 0;
}

#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/FileInputSource.hh>

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>

void check(std::string const& what, bool result)
{
    if (! result)
    {
        std::cout << "FAIL: " << what << std::endl;
    }
}

void do_tests(InputSource* is)
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
    is->unreadCh('Q');
    char b[1];
    b[0] = '\0';
    check("read unread character", 1 == is->read(b, 1));
    check("last offset after read unread", 521 == is->getLastOffset());
    check("got character", 'Q' == b[0]);
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

int main()
{
    // This test is designed to work with a specified input file.
    std::cout << "testing with ClosedFileInputSource\n";
    ClosedFileInputSource cf("input");
    do_tests(&cf);
    std::cout << "testing with FileInputSource\n";
    FileInputSource f;
    f.setFilename("input");
    do_tests(&f);
    std::cout << "all assertions passed" << std::endl;
    return 0;
}

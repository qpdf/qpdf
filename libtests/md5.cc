#include <qpdf/MD5.hh>
#include <qpdf/Pl_MD5.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdio.h>

static void test_string(char const* str)
{
    MD5 a;
    a.encodeString(str);
    a.print();
}

int main(int, char*[])
{
    test_string("");
    test_string("a");
    test_string("abc");
    test_string("message digest");
    test_string("abcdefghijklmnopqrstuvwxyz");
    test_string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghi"
		"jklmnopqrstuvwxyz0123456789");
    test_string("1234567890123456789012345678901234567890"
		"1234567890123456789012345678901234567890");
    MD5 a;
    a.encodeFile("md5.in");
    std::cout << a.unparse() << std::endl;
    MD5 b;
    b.encodeFile("md5.in", 100);
    std::cout << b.unparse() << std::endl;

    std::cout
	<< MD5::checkDataChecksum("900150983cd24fb0d6963f7d28e17f72", "abc", 3)
	<< std::endl
	<< MD5::checkFileChecksum("5f4b4321873433daae578f85c72f9e74", "md5.in")
	<< std::endl
	<< MD5::checkFileChecksum("6f4b4321873433daae578f85c72f9e74", "md5.in")
	<< std::endl
	<< MD5::checkDataChecksum("000150983cd24fb0d6963f7d28e17f72", "abc", 3)
	<< std::endl
	<< MD5::checkFileChecksum("6f4b4321873433daae578f85c72f9e74", "glerbl")
	<< std::endl;


    Pl_Discard d;
    Pl_MD5 p("MD5", &d);
    // Create a second pipeline, protect against finish, and call
    // getHexDigest only once at the end of both passes. Make sure the
    // checksum is that of the input file concatenated to itself. This
    // will require changes to Pl_MD5.cc to prevent finish from
    // calling finalize.
    Pl_MD5 p2("MD5", &d);
    p2.persistAcrossFinish(true);
    for (int i = 0; i < 2; ++i)
    {
	FILE* f = QUtil::safe_fopen("md5.in", "rb");
        // buffer size < size of md5.in
        unsigned char buf[50];
        bool done = false;
        while (! done)
        {
            size_t len = fread(buf, 1, sizeof(buf), f);
            if (len <= 0)
            {
                done = true;
            }
            else
            {
                p.write(buf, len);
                p2.write(buf, len);
                if (i == 1)
                {
                    // Partial digest -- resets after each call to write
                    std::cout << p.getHexDigest() << std::endl;
                }
            }
        }
        fclose(f);
        p.finish();
        p2.finish();
        // Make sure calling getHexDigest twice with no intervening
        // writes results in the same result each time.
        std::cout << p.getHexDigest() << std::endl;
        std::cout << p.getHexDigest() << std::endl;
    }
    std::cout << p2.getHexDigest() << std::endl;

    return 0;
}

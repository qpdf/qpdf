#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <qpdf/QUtil.hh>
#include <string.h>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

void string_conversion_test()
{
    std::cout << QUtil::int_to_string(16059) << std::endl
	      << QUtil::int_to_string(16059, 7) << std::endl
	      << QUtil::int_to_string(16059, -7) << std::endl
	      << QUtil::double_to_string(3.14159) << std::endl
	      << QUtil::double_to_string(3.14159, 3) << std::endl
	      << QUtil::double_to_string(1000.123, -1024) << std::endl
              << QUtil::double_to_string(.1234, 5) << std::endl
              << QUtil::double_to_string(.0001234, 5) << std::endl
              << QUtil::double_to_string(.123456, 5) << std::endl
              << QUtil::double_to_string(.000123456, 5) << std::endl;

    std::string embedded_null = "one";
    embedded_null += '\0';
    embedded_null += "two";
    std::cout << embedded_null.c_str() << std::endl;
    std::cout << embedded_null.length() << std::endl;
    char* tmp = QUtil::copy_string(embedded_null);
    if (memcmp(tmp, embedded_null.c_str(), 7) == 0)
    {
	std::cout << "compare okay" << std::endl;
    }
    else
    {
	std::cout << "compare failed" << std::endl;
    }
    delete [] tmp;
}

void os_wrapper_test()
{
    try
    {
	std::cout << "before remove" << std::endl;
	QUtil::os_wrapper("remove file",
                          remove("/this/file/does/not/exist"));
	std::cout << "after remove" << std::endl;
    }
    catch (std::runtime_error& s)
    {
	std::cout << "exception: " << s.what() << std::endl;
    }
}

void fopen_wrapper_test()
{
    FILE* f = 0;
    try
    {
	std::cout << "before fopen" << std::endl;
	f = QUtil::fopen_wrapper("fopen file",
				 fopen("/this/file/does/not/exist", "r")); // XXXX
	std::cout << "after fopen" << std::endl;
	(void) fclose(f);
    }
    catch (std::runtime_error& s)
    {
	std::cout << "exception: " << s.what() << std::endl;
    }
}

void getenv_test()
{
    std::string val;
    std::cout << "IN_TESTSUITE: " << QUtil::get_env("IN_TESTSUITE", &val)
	      << ": " << val << std::endl;
    // Hopefully this environment variable is not defined.
    std::cout << "HAGOOGAMAGOOGLE: " << QUtil::get_env("HAGOOGAMAGOOGLE")
	      << std::endl;
}

static void print_utf8(unsigned long val)
{
    char t[20];
    sprintf(t, "%lx", val);   // XXXX
    std::string result = QUtil::toUTF8(val);
    std::cout << "0x" << t << " ->";
    if (val < 0xfffe)
    {
	std::cout << " " << result;
    }
    else
    {
	// Emacs has trouble with utf-8 encoding files with characters
	// outside the 16-bit portion, so just show the character
	// values.
	for (std::string::iterator iter = result.begin();
	     iter != result.end(); ++iter)
	{
	    char t[3];
	    sprintf(t, "%02x", static_cast<unsigned char>(*iter)); // XXXX
	    std::cout << " " << t;
	}
    }
    std::cout << std::endl;
}

void to_utf8_test()
{
    print_utf8(0x41UL);
    print_utf8(0xF7UL);
    print_utf8(0x3c0UL);
    print_utf8(0x16059UL);
    print_utf8(0x7fffffffUL);
    try
    {
	print_utf8(0x80000000UL);
    }
    catch (std::runtime_error& e)
    {
	std::cout << "0x80000000: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    try
    {
	string_conversion_test();
	std::cout << "----" << std::endl;
	os_wrapper_test();
	std::cout << "----" << std::endl;
	fopen_wrapper_test();
	std::cout << "----" << std::endl;
	getenv_test();
	std::cout << "----" << std::endl;
	to_utf8_test();
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
    }

    return 0;
}

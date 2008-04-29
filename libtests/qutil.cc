
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <qpdf/QUtil.hh>

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
	      << QUtil::double_to_string(1000.123, -1024) << std::endl;

    try
    {
	// int_to_string bounds error
	std::cout << QUtil::int_to_string(1, 50) << std::endl;
    }
    catch(QEXC::Internal &e)
    {
	std::cout << "exception 1: " << e.unparse() << std::endl;
    }

    try
    {
	// QUtil::int_to_string bounds error
	std::cout << QUtil::int_to_string(1, -50) << std::endl;
    }
    catch(QEXC::Internal &e)
    {
	std::cout << "exception 2: " << e.unparse() << std::endl;
    }

    try
    {
	// QUtil::int_to_string bounds error
	std::cout << QUtil::int_to_string(-1, 49) << std::endl;
    }
    catch(QEXC::Internal &e)
    {
	std::cout << "exception 3: " << e.unparse() << std::endl;
    }


    try
    {
	// QUtil::double_to_string bounds error
	std::cout << QUtil::double_to_string(3.14159, 1024) << std::endl;
    }
    catch(QEXC::Internal &e)
    {
	std::cout << "exception 4: " << e.unparse() << std::endl;
    }

    try
    {
	// QUtil::double_to_string bounds error
	std::cout << QUtil::double_to_string(1000.0, 95) << std::endl;
    }
    catch(QEXC::Internal &e)
    {
	std::cout << "exception 5: " << e.unparse() << std::endl;
    }

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
    int fd = -1;
    try
    {
	std::cout << "before open" << std::endl;
	fd = QUtil::os_wrapper("open file",
			       open("/this/file/does/not/exist", O_RDONLY));
	std::cout << "after open" << std::endl;
	(void) close(fd);
    }
    catch (QEXC::System& s)
    {
	std::cout << "exception: " << s.unparse() << std::endl;
    }
}

void fopen_wrapper_test()
{
    FILE* f = 0;
    try
    {
	std::cout << "before fopen" << std::endl;
	f = QUtil::fopen_wrapper("fopen file",
				 fopen("/this/file/does/not/exist", "r"));
	std::cout << "after fopen" << std::endl;
	(void) fclose(f);
    }
    catch (QEXC::System& s)
    {
	std::cout << "exception: " << s.unparse() << std::endl;
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
    sprintf(t, "%lx", val);
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
	    sprintf(t, "%02x", (unsigned char) (*iter));
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
    catch (QEXC::General& e)
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

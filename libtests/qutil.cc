#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <qpdf/QUtil.hh>
#include <qpdf/PointerHolder.hh>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

template <class int_T>
void test_to_number(char const* str, int_T wanted, bool error,
                    int_T (*fn)(char const*))
{
    bool threw = false;
    bool worked = false;
    int_T result = 0;
    try
    {
        result = fn(str);
        worked = (wanted == result);
    }
    catch (std::runtime_error const&)
    {
        threw = true;
    }
    if (threw)
    {
        if (error)
        {
            std::cout << str << " to int threw: PASSED" << std::endl;
        }
        else
        {
            std::cout << str << " to int threw but wanted "
                      << wanted << std::endl;
        }
    }
    else
    {
        if (worked)
        {
            std::cout << str << " to int: PASSED" << std::endl;
        }
        else
        {
            std::cout << str << " to int failed; got " << result << std::endl;
        }
    }
}

void test_to_int(char const* str, int wanted, bool error)
{
    test_to_number(str, wanted, error, QUtil::string_to_int);
}

void test_to_ll(char const* str, long long wanted, bool error)
{
    test_to_number(str, wanted, error, QUtil::string_to_ll);
}

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
              << QUtil::double_to_string(.000123456, 5) << std::endl
              << QUtil::int_to_string_base(16059, 10) << std::endl
              << QUtil::int_to_string_base(16059, 8) << std::endl
              << QUtil::int_to_string_base(16059, 16) << std::endl;

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

    std::string int_max_str = QUtil::int_to_string(INT_MAX);
    std::string int_min_str = QUtil::int_to_string(INT_MIN);
    long long int_max_plus_1 = static_cast<long long>(INT_MAX) + 1;
    long long int_min_minus_1 = static_cast<long long>(INT_MIN) - 1;
    std::string int_max_plus_1_str = QUtil::int_to_string(int_max_plus_1);
    std::string int_min_minus_1_str = QUtil::int_to_string(int_min_minus_1);
    test_to_int(int_min_str.c_str(), INT_MIN, false);
    test_to_int(int_max_str.c_str(), INT_MAX, false);
    test_to_int(int_max_plus_1_str.c_str(), 0, true);
    test_to_int(int_min_minus_1_str.c_str(), 0, true);
    test_to_int("9999999999999999999999999", 0, true);
    test_to_ll(int_max_plus_1_str.c_str(), int_max_plus_1, false);
    test_to_ll(int_min_minus_1_str.c_str(), int_min_minus_1, false);
    test_to_ll("99999999999999999999999999999999999999999999999999", 0, true);
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
    try
    {
	std::cout << "before fopen" << std::endl;
	FILE* f = QUtil::safe_fopen("/this/file/does/not/exist", "r");
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
    std::string result = QUtil::toUTF8(val);
    std::cout << "0x" << QUtil::int_to_string_base(val, 16) << " ->";
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
	    std::cout << " " << QUtil::int_to_string_base(
                static_cast<int>(static_cast<unsigned char>(*iter)), 16, 2);
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

static void print_utf16(unsigned long val)
{
    std::string result = QUtil::toUTF16(val);
    std::cout << "0x" << QUtil::int_to_string_base(val, 16) << " ->";
    for (std::string::iterator iter = result.begin();
         iter != result.end(); ++iter)
    {
        std::cout << " " << QUtil::int_to_string_base(
            static_cast<int>(static_cast<unsigned char>(*iter)), 16, 2);
    }
    std::cout << std::endl;
}

void to_utf16_test()
{
    print_utf16(0x41UL);
    print_utf16(0xF7UL);
    print_utf16(0x3c0UL);
    print_utf16(0x16059UL);
    print_utf16(0xdeadUL);
    print_utf16(0x7fffffffUL);
    print_utf16(0x80000000UL);
}

void print_whoami(char const* str)
{
    PointerHolder<char> dup(true, QUtil::copy_string(str));
    std::cout << QUtil::getWhoami(dup.getPointer()) << std::endl;
}

void get_whoami_test()
{
    print_whoami("a/b/c/quack1");
    print_whoami("a/b/c/quack2.exe");
    print_whoami("a\\b\\c\\quack3");
    print_whoami("a\\b\\c\\quack4.exe");
}

void assert_same_file(char const* file1, char const* file2, bool expected)
{
    bool actual = QUtil::same_file(file1, file2);
    std::cout << "file1: -" << (file1 ? file1 : "(null)") << "-, file2: -"
              << (file2 ? file2 : "(null)") << "-; same: "
              << actual << ": " << ((actual == expected) ? "PASS" : "FAIL")
              << std::endl;
}

void same_file_test()
{
    try
    {
        fclose(QUtil::safe_fopen("qutil.out", "r"));
        fclose(QUtil::safe_fopen("other-file", "r"));
    }
    catch (std::exception const&)
    {
        std::cout << "same_file_test expects to have qutil.out and other-file"
            " exist in the current directory\n";
        return;
    }
    assert_same_file("qutil.out", "./qutil.out", true);
    assert_same_file("qutil.out", "qutil.out", true);
    assert_same_file("qutil.out", "other-file", false);
    assert_same_file("qutil.out", "", false);
    assert_same_file("qutil.out", 0, false);
    assert_same_file("", "qutil.out", false);
}

void read_lines_from_file_test()
{
    std::list<std::string> lines = QUtil::read_lines_from_file("other-file");
    for (std::list<std::string>::iterator iter = lines.begin();
         iter != lines.end(); ++iter)
    {
        std::cout << *iter << std::endl;
    }
}

void assert_hex_encode(std::string const& input, std::string const& expected)
{
    std::string actual = QUtil::hex_encode(input);
    if (expected != actual)
    {
        std::cout << "hex encode " << input
                  << ": expected = " << expected
                  << "; actual = " << actual
                  << std::endl;
    }
}

void assert_hex_decode(std::string const& input, std::string const& expected)
{
    std::string actual = QUtil::hex_decode(input);
    if (expected != actual)
    {
        std::cout << "hex encode " << input
                  << ": expected = " << expected
                  << "; actual = " << actual
                  << std::endl;
    }
}

void hex_encode_decode_test()
{
    std::cout << "begin hex encode/decode\n";
    assert_hex_encode("", "");
    assert_hex_encode("Potato", "506f7461746f");
    std::string with_null("a\367" "00w");
    with_null[3] = '\0';
    assert_hex_encode(with_null, "61f7300077");
    assert_hex_decode("", "");
    assert_hex_decode("61F7-3000-77", with_null);
    assert_hex_decode("41455", "AEP");
    std::cout << "end hex encode/decode\n";
}

int main(int argc, char* argv[])
{
    try
    {
        std::cout << "---- string conversion" << std::endl;
	string_conversion_test();
	std::cout << "---- os wrapper" << std::endl;
	os_wrapper_test();
	std::cout << "---- fopen" << std::endl;
	fopen_wrapper_test();
	std::cout << "---- getenv" << std::endl;
	getenv_test();
	std::cout << "---- utf8" << std::endl;
	to_utf8_test();
	std::cout << "---- utf16" << std::endl;
	to_utf16_test();
	std::cout << "---- whoami" << std::endl;
	get_whoami_test();
	std::cout << "---- file" << std::endl;
	same_file_test();
	std::cout << "---- lines from file" << std::endl;
	read_lines_from_file_test();
	std::cout << "---- hex encode/decode" << std::endl;
	hex_encode_decode_test();
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
    }

    return 0;
}

#include <qpdf/assert_test.h>

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QUtil.hh>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <locale>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

template <class int_T>
void
test_to_number(
    char const* str, int_T wanted, bool error, int_T (*fn)(char const*))
{
    bool threw = false;
    bool worked = false;
    int_T result = 0;
    std::string msg;
    try {
        result = fn(str);
        worked = (wanted == result);
    } catch (std::runtime_error const& e) {
        threw = true;
        msg = e.what();
    }
    if (threw) {
        if (error) {
            std::cout << str << " to int threw (" << msg << "): PASSED"
                      << std::endl;
        } else {
            std::cout << str << " to int threw but wanted " << wanted
                      << std::endl;
        }
    } else {
        if (worked) {
            std::cout << str << " to int: PASSED" << std::endl;
        } else {
            std::cout << str << " to int failed; got " << result << std::endl;
        }
    }
}

void
test_to_int(char const* str, int wanted, bool error)
{
    test_to_number(str, wanted, error, QUtil::string_to_int);
}

void
test_to_ll(char const* str, long long wanted, bool error)
{
    test_to_number(str, wanted, error, QUtil::string_to_ll);
}

void
test_to_uint(char const* str, unsigned int wanted, bool error)
{
    test_to_number(str, wanted, error, QUtil::string_to_uint);
}

void
test_to_ull(char const* str, unsigned long long wanted, bool error)
{
    test_to_number(str, wanted, error, QUtil::string_to_ull);
}

static void
set_locale()
{
    try {
        // First try a locale known to put commas in numbers.
        std::locale::global(std::locale("en_US.UTF-8"));
    } catch (std::runtime_error&) {
        try {
            // If that fails, fall back to the user's default locale.
            std::locale::global(std::locale(""));
        } catch (std::runtime_error& e) {
            // Ignore this error on Windows without MSVC. We get
            // enough test coverage on other platforms, and mingw
            // seems to have limited locale support (as of
            // 2020-10).
#if !defined(_WIN32) || defined(_MSC_VER)
            throw e;
#endif
        }
    }
}

void
string_conversion_test()
{
    // Make sure the code produces consistent results even if we load
    // a non-C locale.
    set_locale();
    std::cout << QUtil::int_to_string(16059) << std::endl
              << QUtil::int_to_string(16059, 7) << std::endl
              << QUtil::int_to_string(16059, -7) << std::endl
              << QUtil::double_to_string(3.14159, 0, false) << std::endl
              << QUtil::double_to_string(3.14159, 3) << std::endl
              << QUtil::double_to_string(1000.123, -1024, false) << std::endl
              << QUtil::double_to_string(.1234, 5, false) << std::endl
              << QUtil::double_to_string(.0001234, 5) << std::endl
              << QUtil::double_to_string(.123456, 5) << std::endl
              << QUtil::double_to_string(.000123456, 5) << std::endl
              << QUtil::double_to_string(1.01020, 5, true) << std::endl
              << QUtil::double_to_string(1.00000, 5, true) << std::endl
              << QUtil::double_to_string(1, 5, true) << std::endl
              << QUtil::double_to_string(1, 5, false) << std::endl
              << QUtil::double_to_string(10, 2, false) << std::endl
              << QUtil::double_to_string(10, 2, true) << std::endl
              << QUtil::int_to_string_base(16059, 10) << std::endl
              << QUtil::int_to_string_base(16059, 8) << std::endl
              << QUtil::int_to_string_base(16059, 16) << std::endl
              << QUtil::int_to_string_base(5000093552LL, 10) << std::endl;

    std::string embedded_null = "one";
    embedded_null += '\0';
    embedded_null += "two";
    std::cout << embedded_null.c_str() << std::endl;
    std::cout << embedded_null.length() << std::endl;
    char* tmp = QUtil::copy_string(embedded_null);
    if (memcmp(tmp, embedded_null.c_str(), 7) == 0) {
        std::cout << "compare okay" << std::endl;
    } else {
        std::cout << "compare failed" << std::endl;
    }
    delete[] tmp;
    // Also test with make_shared_cstr and make_unique_cstr
    auto tmp2 = QUtil::make_shared_cstr(embedded_null);
    if (memcmp(tmp2.get(), embedded_null.c_str(), 7) == 0) {
        std::cout << "compare okay" << std::endl;
    } else {
        std::cout << "compare failed" << std::endl;
    }
    auto tmp3 = QUtil::make_unique_cstr(embedded_null);
    if (memcmp(tmp3.get(), embedded_null.c_str(), 7) == 0) {
        std::cout << "compare okay" << std::endl;
    } else {
        std::cout << "compare failed" << std::endl;
    }

    std::string int_max_str = QUtil::int_to_string(INT_MAX);
    std::string int_min_str = QUtil::int_to_string(INT_MIN);
    long long int_max_plus_1 = static_cast<long long>(INT_MAX) + 1;
    long long int_min_minus_1 = static_cast<long long>(INT_MIN) - 1;
    std::string int_max_plus_1_str = QUtil::int_to_string(int_max_plus_1);
    std::string int_min_minus_1_str = QUtil::int_to_string(int_min_minus_1);
    std::string small_positive = QUtil::uint_to_string(16059U);
    std::string small_negative = QUtil::int_to_string(-16059);
    test_to_int(int_min_str.c_str(), INT_MIN, false);
    test_to_int(int_max_str.c_str(), INT_MAX, false);
    test_to_int(int_max_plus_1_str.c_str(), 0, true);
    test_to_int(int_min_minus_1_str.c_str(), 0, true);
    test_to_int("9999999999999999999999999", 0, true);
    test_to_ll(int_max_plus_1_str.c_str(), int_max_plus_1, false);
    test_to_ll(int_min_minus_1_str.c_str(), int_min_minus_1, false);
    test_to_ll("99999999999999999999999999999999999999999999999999", 0, true);
    test_to_uint(small_positive.c_str(), 16059U, false);
    test_to_uint(small_negative.c_str(), 0, true);
    test_to_uint("9999999999", 0, true);
    test_to_ull(small_positive.c_str(), 16059U, false);
    test_to_ull(small_negative.c_str(), 0, true);
}

void
os_wrapper_test()
{
    try {
        std::cout << "before remove" << std::endl;
        QUtil::os_wrapper("remove file", remove("/this/file/does/not/exist"));
        std::cout << "after remove" << std::endl;
    } catch (std::runtime_error& s) {
        std::cout << "exception: " << s.what() << std::endl;
    }
}

void
fopen_wrapper_test()
{
    try {
        std::cout << "before fopen" << std::endl;
        FILE* f = QUtil::safe_fopen("/this/file/does/not/exist", "r");
        std::cout << "after fopen" << std::endl;
        (void)fclose(f);
    } catch (QPDFSystemError& s) {
        std::cout << "exception: " << s.what() << std::endl;
        assert(s.getErrno() != 0);
    }

    assert(QUtil::file_can_be_opened("qutil.out"));
    assert(!QUtil::file_can_be_opened("/does/not/exist"));
}

void
getenv_test()
{
    std::string val;
    std::cout << "IN_TESTSUITE: " << QUtil::get_env("IN_TESTSUITE", &val)
              << ": " << val << std::endl;
    // Hopefully this environment variable is not defined.
    std::cout << "HAGOOGAMAGOOGLE: " << QUtil::get_env("HAGOOGAMAGOOGLE")
              << std::endl;
}

static void
print_utf8(unsigned long val)
{
    std::string result = QUtil::toUTF8(val);
    std::cout << "0x" << QUtil::uint_to_string_base(val, 16) << " ->";
    if (val < 0xfffe) {
        std::cout << " " << result;
    } else {
        // Emacs has trouble with utf-8 encoding files with characters
        // outside the 16-bit portion, so just show the character
        // values.
        for (auto const& ch: result) {
            std::cout << " "
                      << QUtil::int_to_string_base(
                             static_cast<int>(static_cast<unsigned char>(ch)),
                             16,
                             2);
        }
    }
    std::cout << std::endl;

    // Boundary conditions for QUtil::get_next_utf8_codepoint, which is
    // also tested indirectly through test_pdf_unicode.cc.
    std::string utf8 = "\xcf\x80\xcf\x30\xEF\xBF\x30\x31\xcf";
    size_t pos = 0;
    bool error = false;
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0x3c0);
    assert(pos == 2);
    assert(!error);
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0xfffd);
    assert(pos == 3);
    assert(error);
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0x30);
    assert(pos == 4);
    assert(!error);
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0xfffd);
    assert(pos == 6);
    assert(error);
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0x30);
    assert(pos == 7);
    assert(!error);
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0x31);
    assert(pos == 8);
    assert(!error);
    assert(QUtil::get_next_utf8_codepoint(utf8, pos, error) == 0xfffd);
    assert(pos == 9);
    assert(error);
}

void
to_utf8_test()
{
    print_utf8(0x41UL);
    print_utf8(0xF7UL);
    print_utf8(0x3c0UL);
    print_utf8(0x16059UL);
    print_utf8(0x7fffffffUL);
    try {
        print_utf8(0x80000000UL);
    } catch (std::runtime_error& e) {
        std::cout << "0x80000000: " << e.what() << std::endl;
    }
}

static void
print_utf16(unsigned long val)
{
    std::string result = QUtil::toUTF16(val);
    std::cout << "0x" << QUtil::uint_to_string_base(val, 16) << " ->";
    for (auto const& ch: result) {
        std::cout << " "
                  << QUtil::int_to_string_base(
                         static_cast<int>(static_cast<unsigned char>(ch)),
                         16,
                         2);
    }
    std::cout << std::endl;
}

void
to_utf16_test()
{
    print_utf16(0x41UL);
    print_utf16(0xF7UL);
    print_utf16(0x3c0UL);
    print_utf16(0x16059UL);
    print_utf16(0xdeadUL);
    print_utf16(0x7fffffffUL);
    print_utf16(0x80000000UL);

    std::string s(QUtil::utf8_to_utf16("\xcf\x80"));
    std::cout << QUtil::utf16_to_utf8(s) << std::endl;
    std::cout << QUtil::utf16_to_utf8(s + ".") << std::endl;
    std::cout << "LE: " << QUtil::utf16_to_utf8("\xff\xfe\xc0\x03")
              << std::endl;
}

void
utf8_to_ascii_test()
{
    char const* input = "\302\277Does \317\200 have fingers?";
    std::cout << input << std::endl
              << QUtil::utf8_to_ascii(input) << std::endl
              << QUtil::utf8_to_ascii(input, '*') << std::endl;
    std::string a = QUtil::utf8_to_win_ansi(input, '*');
    std::string b = QUtil::utf8_to_mac_roman(input, '*');
    std::cout
        << "<"
        << QUtil::int_to_string_base(static_cast<unsigned char>(a.at(0)), 16, 2)
        << ">" << a.substr(1) << std::endl
        << "<"
        << QUtil::int_to_string_base(static_cast<unsigned char>(b.at(0)), 16, 2)
        << ">" << b.substr(1) << std::endl;
}

void
transcoding_test(
    std::string (*to_utf8)(std::string const&),
    std::string (*from_utf8)(std::string const&, char),
    int first,
    int last,
    std::string unknown)
{
    std::string in(" ");
    std::string out;
    std::string back;
    for (int i = first; i <= last; ++i) {
        in.at(0) = static_cast<char>(static_cast<unsigned char>(i));
        out = (*to_utf8)(in);
        std::string wanted = (out == "\xef\xbf\xbd") ? unknown : in;
        back = (*from_utf8)(out, '?');
        if (back != wanted) {
            std::cout << i << ": " << in << " -> " << out << " -> " << back
                      << " (wanted " << wanted << ")" << std::endl;
        }
    }
}

void
check_analyze(std::string const& str, bool has8bit, bool utf8, bool utf16)
{
    bool has_8bit_chars = false;
    bool is_valid_utf8 = false;
    bool is_utf16 = false;
    QUtil::analyze_encoding(str, has_8bit_chars, is_valid_utf8, is_utf16);
    if (!((has_8bit_chars == has8bit) && (is_valid_utf8 == utf8) &&
          (is_utf16 == utf16))) {
        std::cout << "analysis failed: " << str << std::endl;
    }
}

void
print_alternatives(std::string const& str)
{
    std::vector<std::string> result = QUtil::possible_repaired_encodings(str);
    size_t n = result.size();
    for (size_t i = 0; i < n; ++i) {
        std::cout << i << ": " << QUtil::hex_encode(result.at(i)) << std::endl;
    }
}

void
transcoding_test()
{
    transcoding_test(
        &QUtil::pdf_doc_to_utf8, &QUtil::utf8_to_pdf_doc, 127, 160, "\x9f");
    std::cout << "bidirectional pdf doc done" << std::endl;
    transcoding_test(
        &QUtil::pdf_doc_to_utf8, &QUtil::utf8_to_pdf_doc, 24, 31, "?");
    std::cout << "bidirectional pdf doc low done" << std::endl;
    transcoding_test(
        &QUtil::win_ansi_to_utf8, &QUtil::utf8_to_win_ansi, 128, 160, "?");
    std::cout << "bidirectional win ansi done" << std::endl;
    transcoding_test(
        &QUtil::mac_roman_to_utf8, &QUtil::utf8_to_mac_roman, 128, 255, "?");
    std::cout << "bidirectional mac roman done" << std::endl;
    check_analyze("pi = \317\200", true, true, false);
    check_analyze("pi != \317", true, false, false);
    check_analyze("pi != 22/7", false, false, false);
    check_analyze(std::string("\xfe\xff\x00\x51", 4), true, false, true);
    check_analyze(std::string("\xff\xfe\x51\x00", 4), true, false, true);
    std::cout << "analysis done" << std::endl;
    std::string input1("a\302\277b");
    std::string input2("a\317\200b");
    std::string input3("ab");
    std::string output;
    assert(!QUtil::utf8_to_ascii(input1, output));
    assert(!QUtil::utf8_to_ascii(input2, output));
    assert(QUtil::utf8_to_ascii(input3, output));
    assert(QUtil::utf8_to_win_ansi(input1, output));
    assert(!QUtil::utf8_to_win_ansi(input2, output));
    assert(QUtil::utf8_to_win_ansi(input3, output));
    assert(QUtil::utf8_to_mac_roman(input1, output));
    assert(!QUtil::utf8_to_mac_roman(input2, output));
    assert(QUtil::utf8_to_mac_roman(input3, output));
    assert(QUtil::utf8_to_pdf_doc(input1, output));
    assert(!QUtil::utf8_to_pdf_doc(input2, output));
    assert(QUtil::utf8_to_pdf_doc(input3, output));
    std::cout << "alternatives" << std::endl;
    // char     name            mac     win     pdf-doc
    // U+0192   florin          304     203     206
    // U+00A9   copyright       251     251     251
    // U+00E9   eacute          216     351     351
    // U+017E   zcaron          -       236     236
    std::string pdfdoc = "\206\251\351\236";
    std::string utf8 = QUtil::pdf_doc_to_utf8(pdfdoc);
    print_alternatives(pdfdoc);
    print_alternatives(utf8);
    print_alternatives("quack");
    std::cout << "done alternatives" << std::endl;
    // These are characters are either valid in PDFDoc and invalid in
    // UTF-8 or the other way around.
    std::string other("w\x18w\x19w\x1aw\x1bw\x1cw\x1dw\x1ew\x1fw\x7fw");
    // cSpell: ignore xadw
    std::string other_doc = other + "\x9fw\xadw";
    std::cout << QUtil::pdf_doc_to_utf8(other_doc) << std::endl;
    std::string other_utf8 =
        other + QUtil::toUTF8(0x9f) + "w" + QUtil::toUTF8(0xad) + "w";
    std::string other_to_utf8;
    assert(!QUtil::utf8_to_pdf_doc(other_utf8, other_to_utf8));
    std::cout << other_to_utf8 << std::endl;
    std::cout << "done other characters" << std::endl;
}

void
print_whoami(char const* str)
{
    auto dup = QUtil::make_unique_cstr(str);
    std::cout << QUtil::getWhoami(dup.get()) << std::endl;
}

void
get_whoami_test()
{
    print_whoami("a/b/c/quack1");
    print_whoami("a/b/c/quack2.exe");
    print_whoami("a\\b\\c\\quack3");
    print_whoami("a\\b\\c\\quack4.exe");
}

void
assert_same_file(char const* file1, char const* file2, bool expected)
{
    bool actual = QUtil::same_file(file1, file2);
    std::cout << "file1: -" << (file1 ? file1 : "(null)") << "-, file2: -"
              << (file2 ? file2 : "(null)") << "-; same: " << actual << ": "
              << ((actual == expected) ? "PASS" : "FAIL") << std::endl;
}

void
same_file_test()
{
    try {
        fclose(QUtil::safe_fopen("qutil.out", "r"));
        fclose(QUtil::safe_fopen("other-file", "r"));
    } catch (std::exception const&) {
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

void
path_test()
{
    auto check = [](bool print, std::string const& a, std::string const& b) {
        auto result = QUtil::path_basename(a);
        if (print) {
            std::cout << a << " -> " << result << std::endl;
        }
        assert(result == b);
    };

#ifdef _WIN32
    check(false, "asdf\\qwer", "qwer");
    check(false, "asdf\\qwer/\\", "qwer");
#endif
    check(true, "////", "/");
    check(true, "a/b/cdef", "cdef");
    check(true, "a/b/cdef/", "cdef");
    check(true, "/", "/");
    check(true, "", "");
    check(true, "quack", "quack");
}

void
read_from_file_test()
{
    std::list<std::string> lines = QUtil::read_lines_from_file("other-file");
    for (auto const& line: lines) {
        std::cout << line << std::endl;
    }
    // Test the other versions and make sure we get the same results
    {
        std::ifstream infs("other-file", std::ios_base::binary);
        assert(QUtil::read_lines_from_file(infs) == lines);
        FILE* fp = QUtil::safe_fopen("other-file", "rb");
        assert(QUtil::read_lines_from_file(fp) == lines);
        fclose(fp);
    }

    // Test with EOL preservation
    std::list<std::string> lines2 =
        QUtil::read_lines_from_file("other-file", true);
    auto line = lines2.begin();
    assert(37 == (*line).length());
    assert('.' == (*line).at(35));
    assert('\n' == (*line).at(36));
    ++line;
    assert(24 == (*line).length());
    assert('.' == (*line).at(21));
    assert('\r' == (*line).at(22));
    assert('\n' == (*line).at(23));
    ++line;
    assert(24591 == (*line).length());
    assert('.' == (*line).at(24589));
    assert('\n' == (*line).at(24590));
    // Test the other versions and make sure we get the same results
    {
        std::ifstream infs("other-file", std::ios_base::binary);
        assert(QUtil::read_lines_from_file(infs, true) == lines2);
        FILE* fp = QUtil::safe_fopen("other-file", "rb");
        assert(QUtil::read_lines_from_file(fp, true) == lines2);
        fclose(fp);
    }

    std::shared_ptr<char> buf;
    size_t size = 0;
    QUtil::read_file_into_memory("other-file", buf, size);
    std::cout << "read " << size << " bytes" << std::endl;
    char const* p = buf.get();
    assert(size == 24652);
    assert(memcmp(p, "This file is used for qutil testing.", 36) == 0);
    assert(p[59] == static_cast<char>(13));
    assert(memcmp(p + 24641, "very long.", 10) == 0);
    Pl_Buffer b2("buffer");
    // QUtil::file_provider also exercises QUtil::pipe_file
    QUtil::file_provider("other-file")(&b2);
    auto buf2 = b2.getBufferSharedPointer();
    assert(buf2->getSize() == size);
    assert(memcmp(buf2->getBuffer(), p, size) == 0);
}

void
assert_hex_encode(std::string const& input, std::string const& expected)
{
    std::string actual = QUtil::hex_encode(input);
    if (expected != actual) {
        std::cout << "hex encode " << input << ": expected = " << expected
                  << "; actual = " << actual << std::endl;
    }
}

void
assert_hex_decode(std::string const& input, std::string const& expected)
{
    std::string actual = QUtil::hex_decode(input);
    if (expected != actual) {
        std::cout << "hex encode " << input << ": expected = " << expected
                  << "; actual = " << actual << std::endl;
    }
}

void
hex_encode_decode_test()
{
    std::cout << "begin hex encode/decode\n";
    assert_hex_encode("", "");
    assert_hex_encode("Potato", "506f7461746f");
    std::string with_null("a\367"
                          "00w");
    with_null[3] = '\0';
    assert_hex_encode(with_null, "61f7300077");
    assert_hex_decode("", "");
    assert_hex_decode("61F7-3000-77", with_null);
    assert_hex_decode("41455", "AEP");
    std::cout << "end hex encode/decode\n";
}

static void
assert_no_file(char const* filename)
{
    try {
        fclose(QUtil::safe_fopen(filename, "r"));
        assert(false);
    } catch (QPDFSystemError&) {
    }
}

void
rename_delete_test()
{
    std::shared_ptr<char> buf;
    size_t size = 0;

    try {
        QUtil::remove_file("old\xcf\x80");
    } catch (QPDFSystemError&) {
    }
    assert_no_file("old\xcf\x80");
    std::cout << "create file" << std::endl;
    ;
    FILE* f1 = QUtil::safe_fopen("old\xcf\x80", "w");
    fprintf(f1, "one");
    fclose(f1);
    QUtil::read_file_into_memory("old\xcf\x80", buf, size);
    assert(memcmp(buf.get(), "one", 3) == 0);
    std::cout << "rename file" << std::endl;
    ;
    QUtil::rename_file("old\xcf\x80", "old\xcf\x80.~tmp");
    QUtil::read_file_into_memory("old\xcf\x80.~tmp", buf, size);
    assert(memcmp(buf.get(), "one", 3) == 0);
    assert_no_file("old\xcf\x80");
    std::cout << "create file" << std::endl;
    ;
    f1 = QUtil::safe_fopen("old\xcf\x80", "w");
    fprintf(f1, "two");
    fclose(f1);
    std::cout << "rename over existing" << std::endl;
    ;
    QUtil::rename_file("old\xcf\x80", "old\xcf\x80.~tmp");
    QUtil::read_file_into_memory("old\xcf\x80.~tmp", buf, size);
    assert(memcmp(buf.get(), "two", 3) == 0);
    assert_no_file("old\xcf\x80");
    std::cout << "delete file" << std::endl;
    ;
    QUtil::remove_file("old\xcf\x80.~tmp");
    assert_no_file("old\xcf\x80");
    assert_no_file("old\xcf\x80.~tmp");
}

void
timestamp_test()
{
    auto check = [](QUtil::QPDFTime const& t) {
        std::string pdf = QUtil::qpdf_time_to_pdf_time(t);
        std::string iso8601 = QUtil::qpdf_time_to_iso8601(t);
        std::cout << pdf << std::endl << iso8601 << std::endl;
        QUtil::QPDFTime t2;
        std::string iso8601_2;
        assert(QUtil::pdf_time_to_qpdf_time(pdf, &t2));
        assert(QUtil::qpdf_time_to_pdf_time(t2) == pdf);
        assert(QUtil::pdf_time_to_iso8601(pdf, iso8601_2));
        assert(iso8601 == iso8601_2);
    };
    check(QUtil::QPDFTime(2021, 2, 9, 14, 49, 25, 300));
    check(QUtil::QPDFTime(2021, 2, 10, 1, 19, 25, -330));
    check(QUtil::QPDFTime(2021, 2, 9, 19, 19, 25, 0));
    assert(!QUtil::pdf_time_to_qpdf_time("potato"));
    assert(QUtil::pdf_time_to_qpdf_time("D:20210211064743Z"));
    assert(QUtil::pdf_time_to_qpdf_time("D:20210211064743-05'00'"));
    assert(QUtil::pdf_time_to_qpdf_time("D:20210211064743+05'30'"));
    assert(QUtil::pdf_time_to_qpdf_time("D:20210211064743"));
    // Round trip on the current time without actually printing it.
    // Manual testing was done to ensure that we are actually getting
    // back the current time in various time zones.
    assert(QUtil::pdf_time_to_qpdf_time(
        QUtil::qpdf_time_to_pdf_time(QUtil::get_current_qpdf_time())));
}

void
is_long_long_test()
{
    auto check = [](char const* s, bool v) {
        if (QUtil::is_long_long(s) != v) {
            std::cout << "failed: " << s << std::endl;
        }
    };
    check("12312312", true);
    check("12312312.34", false);
    check("-12312312", true);
    check("-12312312.34", false);
    check("1e2", false);
    check("9223372036854775807", true);
    check("9223372036854775808", false);
    check("-9223372036854775808", true);
    check("-9223372036854775809", false);
    check("123123123123123123123123123123123123", false);
    check("potato", false);
    check("0123", false);
    std::cout << "done" << std::endl;
}

int
main(int argc, char* argv[])
{
    try {
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
        std::cout << "---- utf8_to_ascii" << std::endl;
        utf8_to_ascii_test();
        std::cout << "---- transcoding" << std::endl;
        transcoding_test();
        std::cout << "---- whoami" << std::endl;
        get_whoami_test();
        std::cout << "---- file" << std::endl;
        same_file_test();
        std::cout << "---- path" << std::endl;
        path_test();
        std::cout << "---- read from file" << std::endl;
        read_from_file_test();
        std::cout << "---- hex encode/decode" << std::endl;
        hex_encode_decode_test();
        std::cout << "---- rename/delete" << std::endl;
        rename_delete_test();
        std::cout << "---- timestamp" << std::endl;
        timestamp_test();
        std::cout << "---- is_long_long" << std::endl;
        is_long_long_test();
    } catch (std::exception& e) {
        std::cout << "unexpected exception: " << e.what() << std::endl;
    }

    return 0;
}

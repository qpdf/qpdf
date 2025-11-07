#include <qpdf/assert_test.h>

// This program tests miscellaneous object handle functionality

#include <qpdf/QPDF.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QUtil.hh>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " n filename1 [arg2]" << '\n';
    exit(2);
}

#define assert_compare_numbers(expected, expr) compare_numbers(#expr, expected, expr)

template <typename T1, typename T2>
static void
compare_numbers(char const* description, T1 const& expected, T2 const& actual)
{
    if (expected != actual) {
        std::cerr << description << ": expected = " << expected << "; actual = " << actual << '\n';
    }
}

static void
test_0(QPDF& pdf, char const* arg2)
{
    // Test int size checks. This test will fail if int and long long are the same size.
    QPDFObjectHandle t = pdf.getTrailer();
    unsigned long long q1_l = 3ULL * QIntC::to_ulonglong(INT_MAX);
    long long q1 = QIntC::to_longlong(q1_l);
    long long q2_l = 3LL * QIntC::to_longlong(INT_MIN);
    long long q2 = QIntC::to_longlong(q2_l);
    unsigned int q3_i = UINT_MAX;
    long long q3 = QIntC::to_longlong(q3_i);
    t.replaceKey("/Q1", QPDFObjectHandle::newInteger(q1));
    t.replaceKey("/Q2", QPDFObjectHandle::newInteger(q2));
    t.replaceKey("/Q3", QPDFObjectHandle::newInteger(q3));
    assert_compare_numbers(q1, t.getKey("/Q1").getIntValue());
    assert_compare_numbers(q1_l, t.getKey("/Q1").getUIntValue());
    assert_compare_numbers(INT_MAX, t.getKey("/Q1").getIntValueAsInt());
    try {
        assert_compare_numbers(0u, QPDFObjectHandle::newNull().getUIntValueAsUInt());
        std::cerr << "convert null to uint did not throw\n";
    } catch (QPDFExc const&) {
        std::cerr << "caught expected type error\n";
    }
    assert_compare_numbers(std::numeric_limits<int8_t>::max(), Integer(q1).value<int8_t>());
    assert_compare_numbers(std::numeric_limits<int8_t>::min(), Integer(-q1).value<int8_t>());
    try {
        int8_t q1_8 = Integer(q1);
        std::cerr << "q1_8: " << std::to_string(q1_8) << '\n';
    } catch (std::overflow_error const&) {
        std::cerr << "caught expected int8_t overflow error\n";
    }
    try {
        int8_t q1_8 = Integer(-q1);
        std::cerr << "q1_8: " << std::to_string(q1_8) << '\n';
    } catch (std::underflow_error const&) {
        std::cerr << "caught expected int8_t underflow error\n";
    }
    assert_compare_numbers(std::numeric_limits<uint8_t>::max(), Integer(q1).value<uint8_t>());
    assert_compare_numbers(0, Integer(-q1).value<uint8_t>());
    try {
        uint8_t q1_u8 = Integer(q1);
        std::cerr << "q1_u8: " << std::to_string(q1_u8) << '\n';
    } catch (std::overflow_error const&) {
        std::cerr << "caught expected uint8_t overflow error\n";
    }
    try {
        uint8_t q1_u8 = Integer(-q1);
        std::cerr << "q1_u8: " << std::to_string(q1_u8) << '\n';
    } catch (std::underflow_error const&) {
        std::cerr << "caught expected uint8_t underflow error\n";
    }
    assert_compare_numbers(UINT_MAX, t.getKey("/Q1").getUIntValueAsUInt());
    assert_compare_numbers(q2_l, t.getKey("/Q2").getIntValue());
    assert_compare_numbers(0U, t.getKey("/Q2").getUIntValue());
    assert_compare_numbers(INT_MIN, t.getKey("/Q2").getIntValueAsInt());
    assert_compare_numbers(0U, t.getKey("/Q2").getUIntValueAsUInt());
    assert_compare_numbers(INT_MAX, t.getKey("/Q3").getIntValueAsInt());
    assert_compare_numbers(UINT_MAX, t.getKey("/Q3").getUIntValueAsUInt());
}

void
runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    std::set<int> ignore_filename = {};

    QPDF pdf;
    std::shared_ptr<char> file_buf;
    FILE* filep = nullptr;
    if (ignore_filename.contains(n)) {
        // Ignore filename argument entirely
    } else {
        size_t size = 0;
        QUtil::read_file_into_memory(filename1, file_buf, size);
        pdf.processMemoryFile(filename1, file_buf.get(), size);
    }

    std::map<int, void (*)(QPDF&, char const*)> test_functions = {
        {0, test_0},
    };

    auto fn = test_functions.find(n);
    if (fn == test_functions.end()) {
        throw std::runtime_error(std::string("invalid test ") + QUtil::int_to_string(n));
    }
    (fn->second)(pdf, arg2);

    if (filep) {
        fclose(filep);
    }
    std::cout << "test " << n << " done" << '\n';
}

int
main(int argc, char* argv[])
{
    QUtil::setLineBuf(stdout);
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if ((argc < 3) || (argc > 4)) {
        usage();
    }

    try {
        int n = QUtil::string_to_int(argv[1]);
        char const* filename1 = argv[2];
        char const* arg2 = argv[3];
        runtest(n, filename1, arg2);
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        exit(2);
    }

    return 0;
}

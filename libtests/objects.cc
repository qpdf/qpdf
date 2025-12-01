#include <qpdf/assert_test.h>

// This program tests miscellaneous object handle functionality

#include <qpdf/QPDF.hh>

#include <qpdf/Pl_Discard.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/global.hh>

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
    } catch (QPDFExc const&) {
        std::cerr << "caught expected type error\n";
    }
    assert_compare_numbers(std::numeric_limits<int8_t>::max(), Integer(q1).value<int8_t>());
    assert_compare_numbers(std::numeric_limits<int8_t>::min(), Integer(-q1).value<int8_t>());
    try {
        [[maybe_unused]] int8_t q1_8 = Integer(q1);
    } catch (std::overflow_error const&) {
        std::cerr << "caught expected int8_t overflow error\n";
    }
    try {
        [[maybe_unused]] int8_t q1_8 = Integer(-q1);
    } catch (std::underflow_error const&) {
        std::cerr << "caught expected int8_t underflow error\n";
    }
    assert_compare_numbers(std::numeric_limits<uint8_t>::max(), Integer(q1).value<uint8_t>());
    assert_compare_numbers(0, Integer(-q1).value<uint8_t>());
    try {
        [[maybe_unused]] uint8_t q1_u8 = Integer(q1);
    } catch (std::overflow_error const&) {
        std::cerr << "caught expected uint8_t overflow error\n";
    }
    try {
        [[maybe_unused]] uint8_t q1_u8 = Integer(-q1);
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

static void
test_1(QPDF& pdf, char const* arg2)
{
    // Test new dictionary methods.
    using namespace qpdf;
    auto d = Dictionary({{"/A", {}}, {"/B", Null()}, {"/C", Dictionary::empty()}});

    // contains
    assert(!d.contains("/A"));
    assert(!d.contains("/B"));
    assert(d.contains("/C"));

    auto i = Integer(42);
    assert(!i.contains("/A"));

    // at
    assert(!d.at("/A"));
    assert(d.at("/B"));
    assert(d.at("/B").null());
    assert(d.at("/C"));
    assert(!d.at("/C").null());
    d.at("/C") = Integer(42);
    assert(d.at("/C") == 42);
    assert(!d.at("/D"));
    assert(d.at("/D").null());
    assert(QPDFObjectHandle(d).getDictAsMap().contains("/D"));
    assert(QPDFObjectHandle(d).getDictAsMap().size() == 4);

    bool thrown = false;
    try {
        i.at("/A");
    } catch (std::runtime_error const&) {
        thrown = true;
    }
    assert(thrown);

    // find
    assert(!d.find("/A"));
    assert(d.find("/B"));
    assert(d.find("/B").null());
    assert(d.find("/C"));
    assert(Integer(d.find("/C")) == 42);
    d.find("/C") = Name("/DontPanic");
    assert(Name(d.find("/C")) == "/DontPanic");
    assert(!d.find("/E"));
    assert(!QPDFObjectHandle(d).getDictAsMap().contains("/E"));
    assert(QPDFObjectHandle(d).getDictAsMap().size() == 4);

    // replace
    assert(!i.replace("/A", Name("/DontPanic")));
    Dictionary di = i.oh();
    thrown = false;
    try {
        di.replace("/A", Name("/DontPanic"));
    } catch (std::runtime_error const&) {
        thrown = true;
    }
    assert(thrown);
    d.replace("/C", Integer(42));
    assert(Integer(d["/C"]) == 42);
    assert(QPDFObjectHandle(d).getDictAsMap().size() == 4);
}

static void
test_2(QPDF& pdf, char const* arg2)
{
    // Test global limits.
    using namespace qpdf::global::options;
    using namespace qpdf::global::limits;

    // Check default values
    assert(parser_max_nesting() == 499);
    assert(parser_max_errors() == 15);
    assert(parser_max_container_size() == std::numeric_limits<uint32_t>::max());
    assert(parser_max_container_size_damaged() == 5'000);
    assert(max_stream_filters() == 25);
    assert(default_limits());

    // Test disabling optional default limits
    default_limits(false);
    assert(parser_max_nesting() == 499);
    assert(parser_max_errors() == 0);
    assert(parser_max_container_size() == std::numeric_limits<uint32_t>::max());
    assert(parser_max_container_size_damaged() == std::numeric_limits<uint32_t>::max());
    assert(max_stream_filters() == std::numeric_limits<uint32_t>::max());
    assert(!default_limits());

    // Check disabling default limits is irreversible
    default_limits(true);
    assert(!default_limits());

    // Test setting limits
    parser_max_nesting(11);
    parser_max_errors(12);
    parser_max_container_size(13);
    parser_max_container_size_damaged(14);
    max_stream_filters(15);

    assert(parser_max_nesting() == 11);
    assert(parser_max_errors() == 12);
    assert(parser_max_container_size() == 13);
    assert(parser_max_container_size_damaged() == 14);
    assert(max_stream_filters() == 15);

    // Check disabling default limits does not override explicit limits
    default_limits(false);
    assert(parser_max_nesting() == 11);
    assert(parser_max_errors() == 12);
    assert(parser_max_container_size() == 13);
    assert(parser_max_container_size_damaged() == 14);
    assert(max_stream_filters() == 15);

    // Test parameter checking
    QUtil::handle_result_code(qpdf_r_ok, "");
    bool thrown = false;
    try {
        qpdf::global::handle_result(qpdf_r_success_mask);
    } catch (std::logic_error const&) {
        thrown = true;
    }
    assert(thrown);
    thrown = false;
    try {
        qpdf::global::get_uint32(qpdf_param_e(42));
    } catch (std::logic_error const&) {
        thrown = true;
    }
    assert(thrown);
    thrown = false;
    try {
        qpdf::global::set_uint32(qpdf_param_e(42), 42);
    } catch (std::logic_error const&) {
        thrown = true;
    }
    assert(thrown);

    /* Test limit errors */
    assert(qpdf::global::limit_errors() == 0);
    QPDFObjectHandle::parse("[[[[]]]]");
    assert(qpdf::global::limit_errors() == 0);
    parser_max_nesting(3);
    try {
        QPDFObjectHandle::parse("[[[[[]]]]]");
    } catch (std::exception&) {
    }
    assert(qpdf::global::limit_errors() == 1);
    try {
        QPDFObjectHandle::parse("[[[[[]]]]]");
    } catch (std::exception&) {
    }
    assert(qpdf::global::limit_errors() == 2);

    // Test max_stream_filters
    QPDF qpdf;
    qpdf.emptyPDF();
    auto s = qpdf.newStream("\x01\x01\x01A");
    s.getDict().replace("/Filter", Array({Name("/RL"), Name("/RL"), Name("/RL")}));
    Pl_Discard p;
    auto x = s.pipeStreamData(&p, 0, qpdf_dl_all, true);
    assert(x);
    max_stream_filters(2);
    assert(!s.pipeStreamData(&p, 0, qpdf_dl_all, true));
    max_stream_filters(3);
    assert(s.pipeStreamData(&p, 0, qpdf_dl_all, true));

    // Test global settings using the QPDFJob interface
    QPDFJob j;
    j.config()
        ->inputFile("minimal.pdf")
        ->global()
        ->parserMaxNesting("111")
        ->parserMaxErrors("112")
        ->parserMaxContainerSize("113")
        ->parserMaxContainerSizeDamaged("114")
        ->maxStreamFilters("115")
        ->noDefaultLimits()
        ->endGlobal()
        ->outputFile("a.pdf");
    auto qpdf_uptr = j.createQPDF();
    assert(parser_max_nesting() == 111);
    assert(parser_max_errors() == 112);
    assert(parser_max_container_size() == 113);
    assert(parser_max_container_size_damaged() == 114);
    assert(max_stream_filters() == 115);
    assert(!default_limits());

    // Test global settings using the JobJSON
    QPDFJob jj;
    jj.initializeFromJson(R"(
        {
            "inputFile": "minimal.pdf",
            "global": {
                "parserMaxNesting": "211",
                "parserMaxErrors": "212",
                "parserMaxContainerSize": "213",
                "parserMaxContainerSizeDamaged": "214",
                "maxStreamFilters": "215",
                "noDefaultLimits": ""
            },
            "outputFile": "a.pdf"
        }
    )");
    qpdf_uptr = jj.createQPDF();
    assert(parser_max_nesting() == 211);
    assert(parser_max_errors() == 212);
    assert(parser_max_container_size() == 213);
    assert(parser_max_container_size_damaged() == 214);
    assert(max_stream_filters() == 215);
    assert(!default_limits());
}

void
runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    std::set<int> ignore_filename = {1, 2};

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
        {0, test_0}, {1, test_1}, {2, test_2}};

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

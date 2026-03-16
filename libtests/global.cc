#include <qpdf/assert_test.h>

// This program tests global limits and settings

#include <qpdf/QPDF.hh>

#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/global.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>

using namespace qpdf::global;
using namespace qpdf::global::options;
using namespace qpdf::global::limits;
using namespace qpdf::util;

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " n filename1 [arg2]" << '\n';
    exit(2);
}

// Test global limits.
static void
test_0(QPDF&, char const*)
{
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
    assert(throws<std::logic_error>([]() { handle_result(qpdf_r_success_mask); }));
    assert(throws<std::logic_error>([]() { get_uint32(qpdf_param_e(42)); }));
    assert(throws<std::logic_error>([]() { set_uint32(qpdf_param_e(42), 42); }));

    /* Test limit errors */
    assert(limit_errors() == 0);
    QPDFObjectHandle::parse("[[[[]]]]");
    assert(limit_errors() == 0);
    parser_max_nesting(3);
    assert(throws<std::exception>([]() { QPDFObjectHandle::parse("[[[[[]]]]]"); }));
    assert(limit_errors() == 1);
    assert(throws<std::exception>([]() { QPDFObjectHandle::parse("[[[[[]]]]]"); }));
    assert(limit_errors() == 2);

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
    (void)jj.createQPDF();
    assert(parser_max_nesting() == 211);
    assert(parser_max_errors() == 212);
    assert(parser_max_container_size() == 213);
    assert(parser_max_container_size_damaged() == 214);
    assert(max_stream_filters() == 215);
    assert(!default_limits());
}

// Test filter limits and options.
static void
test_1(QPDF&, char const*)
{
    // Check defaults for DCT limits (0 == no limit)
    assert(dct_max_memory() == 0);
    assert(dct_max_progressive_scans() == 0);

    // DCT-specific limits
    dct_max_memory(123456);
    dct_max_progressive_scans(7);

    assert(dct_max_memory() == 123456);
    assert(get_uint32(qpdf_p_dct_max_memory) == 123456);
    assert(dct_max_progressive_scans() == 7);
    assert(get_uint32(qpdf_p_dct_max_progressive_scans) == 7);
    Pl_DCT::setMemoryLimit(12345);
    assert(dct_max_memory() == 12345);
    Pl_DCT::setScanLimit(8);
    assert(dct_max_progressive_scans() == 8);

    // Check disabling default limits does not override filter limits
    default_limits(false);

    assert(dct_max_memory() == 12345);
    assert(dct_max_progressive_scans() == 8);
}

// Test C-API setters
static void
test_2(QPDF&, char const*)
{
    // DCT-specific limits
    set_uint32(qpdf_p_dct_max_memory, 1234);
    assert(dct_max_memory() == 1234);
    set_uint32(qpdf_p_dct_max_progressive_scans, 6);
    assert(dct_max_progressive_scans() == 6);
}

void
runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    std::set<int> ignore_filename = {0, 1, 2};

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
        char const* arg2 = (argc >= 4) ? argv[3] : "";
        runtest(n, filename1, arg2);
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        exit(2);
    }

    return 0;
}

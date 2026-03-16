#include <qpdf/assert_test.h>

// This program tests miscellaneous object handle functionality

#include <qpdf/QPDF.hh>

#include <qpdf/Pl_Discard.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>
#include <qpdf/global.hh>

using namespace qpdf::util;

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

    assert(throws<std::runtime_error>([&]() { i.at("/A"); }));

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
    assert(throws<std::runtime_error>([&]() { di.replace("/A", Name("/DontPanic")); }));
    d.replace("/C", Integer(42));
    assert(Integer(d["/C"]) == 42);
    assert(QPDFObjectHandle(d).getDictAsMap().size() == 4);
}

// test equivalent_to
static void
test_3(QPDF& pdf, char const* arg2)
{
    // Scenario 1: Basic Equality: Name, Scalars
    {
        auto name = "/Test"_qpdf;
        auto integer = Integer(42);
        assert(name.equivalent_to("/Test"_qpdf));
        assert(!name.equivalent_to(integer));
    }
    // Scenario 2: Numeric Types (Int vs Real)
    {
        auto integer = Integer(1);
        auto real = QPDFObjectHandle::newReal("1.0");
        assert(real.equivalent_to(integer));
        assert(integer.equivalent_to(real));
    }
    // Scenario 3: Array Order Sensitivity
    {
        auto a1 = "[1 2]"_qpdf;
        auto a2 = "[2 1]"_qpdf;
        assert(!a1.equivalent_to(a2));
        assert(!a2.equivalent_to(a1));
    }
    // Scenario 4: Dictionary Key Order Insensitivity
    {
        auto d1 = "<< /A 1 >>"_qpdf;
        d1.replaceKey("/B", Integer(2));
        auto d2 = "<< /B 2 >>"_qpdf;
        d2.replaceKey("/A", Integer(1));
        assert(d1.equivalent_to(d2));
        assert(d2.equivalent_to(d1));
    }
    // Scenario 5: Direct vs Indirect Equality
    {
        auto obj = Integer(100);
        auto indirect = pdf.makeIndirectObject(Integer(100));
        assert(obj.equivalent_to(indirect));
        assert(indirect.equivalent_to(obj));
    }
    // Scenario 6: Diamond Graph Isomorphism
    {
        auto d = pdf.makeIndirectObject(Integer(99));
        auto b = pdf.makeIndirectObject(QPDFObjectHandle::newArray({d}));
        auto c = pdf.makeIndirectObject(QPDFObjectHandle::newArray({d}));
        assert(Array({b, c}).equivalent_to(Array({b, c})));
    }
    // Scenario 7: Circular References (Self-Loop): Compares as False
    {
        auto a1 = pdf.makeIndirectObject("[]"_qpdf);
        a1.appendItem(a1);
        auto a2 = pdf.makeIndirectObject("[]"_qpdf);
        a2.appendItem(a2);
        // The implementation rejects if there is any cycle, for performance reasons
        assert(!a1.equivalent_to(a2));
    }
    // Scenario 8: Cross-Document Comparison (Objects from Different QPDF Instances)
    {
        QPDF pdf2;
        pdf2.emptyPDF();
        auto a1 = pdf.makeIndirectObject("[1]"_qpdf);
        auto a2 = pdf2.makeIndirectObject("[1]"_qpdf);
        auto a3 = pdf2.makeIndirectObject("[2]"_qpdf);
        assert(a1.equivalent_to(a2));  // Same content, different documents
        assert(a2.equivalent_to(a1));  // Same content, different documents
        assert(!a1.equivalent_to(a3)); // Different content, different documents
        assert(!a3.equivalent_to(a1)); // Different content, different documents
    }
    // Scenario 9: Stream Content: Match
    {
        assert(pdf.newStream("Stream data").equivalent_to(pdf.newStream("Stream data")));
    }
    // Scenario 10: Stream Content: Mismatch
    {
        auto s1 = pdf.newStream("Data A");
        auto s2 = pdf.newStream("Data B");
        assert(!s1.equivalent_to(s2));
        assert(!s2.equivalent_to(s1));
    }
    // Scenario 11: Stream Dictionary Differences
    {
        auto s1 = pdf.newStream("same");
        auto s2 = pdf.newStream("same");
        s2.getDict().replaceKey("/Extra", QPDFObjectHandle::newName("/Value"));
        assert(!s1.equivalent_to(s2));
        assert(!s2.equivalent_to(s1));
    }
    // Scenario 12: J.3.6: Absent Keys vs Null
    {
        auto d0 = Dictionary::empty();
        auto d1 = "<</Present null>>"_qpdf;
        auto d2 = "<</Present << >> >>"_qpdf;
        auto d3 = "<</Present [] >>"_qpdf;
        assert(d0.equivalent_to(d1));
        assert(d1.equivalent_to(d0));
        assert(!d0.equivalent_to(d2));
        assert(!d2.equivalent_to(d0));
        assert(!d0.equivalent_to(d3));
        assert(!d3.equivalent_to(d0));
        assert(!d1.equivalent_to(d2));
        assert(!d2.equivalent_to(d1));
        assert(!d1.equivalent_to(d3));
        assert(!d3.equivalent_to(d1));
    }
    // Scenario 13: String Syntax: Hex vs Literal (Annex J)
    {
        auto literal = "(A)"_qpdf;
        auto hex = "<41>"_qpdf;
        assert(literal.equivalent_to(hex));
        assert(hex.equivalent_to(literal));
    }
    // Scenario 14: Name Syntax (Parser) vs Distinct Names (Model)
    {
        auto name1 = "/Name"_qpdf;
        auto name2 = "/Na#6d#65"_qpdf;
        assert(name1.equivalent_to(name2));
        assert(name2.equivalent_to(name1));
    }
    // Scenario 15: Annex J Oddities: Keys, Octals, and Zeros
    {
        auto key1 = "<< /Key 1 >>"_qpdf;
        auto key2 = "<< /K#65#79 1 >>"_qpdf;
        auto lit_A = "(A)"_qpdf;
        auto oct_A = "(\\101)"_qpdf;
        auto zero_i = Integer(0);
        auto zero_r = QPDFObjectHandle::newReal("-0.0");
        auto r1 = QPDFObjectHandle::newReal("12.345");
        auto r2 = QPDFObjectHandle::newReal("12.345000000000000");
        auto i12 = Integer(12);
        // note: we rely on double rounding here
        auto r_lo = QPDFObjectHandle::newReal("11.99999999999999999999999999999999");
        auto i12b = Integer(12);
        auto r_hi = QPDFObjectHandle::newReal("12.00000000000000000000000000000000");
        auto i1 = Integer(1);
        auto r_1 = QPDFObjectHandle::newReal("1.");
        assert(key1.equivalent_to(key2));
        assert(key2.equivalent_to(key1));
        assert(lit_A.equivalent_to(oct_A));
        assert(oct_A.equivalent_to(lit_A));
        assert(zero_i.equivalent_to(zero_r));
        assert(zero_r.equivalent_to(zero_i));
        assert(r1.equivalent_to(r2));
        assert(r2.equivalent_to(r1));
        assert(i12.equivalent_to(r_lo));
        assert(r_lo.equivalent_to(i12));
        assert(i12b.equivalent_to(r_hi));
        assert(r_hi.equivalent_to(i12b));
        assert(i1.equivalent_to(r_1));
        assert(r_1.equivalent_to(i1));
    }
    // Scenario 16: Nested Containers
    {
        assert(Dictionary({{"/K", "[5]"_qpdf}}).equivalent_to(Dictionary({{"/K", "[5]"_qpdf}})));
    }
    // Scenario 17: Boolean and Null mismatch
    {
        auto b_true = QPDFObjectHandle::newBool(true);
        auto b_false = QPDFObjectHandle::newBool(false);
        auto null = QPDFObjectHandle::newNull();
        auto null2 = QPDFObjectHandle::newNull();
        auto one = Integer(1);
        auto zero = Integer(0);
        assert(null.equivalent_to(null));
        assert(null.equivalent_to(null2));
        assert(!b_true.equivalent_to(b_false));
        assert(!b_true.equivalent_to(null));
        assert(!b_true.equivalent_to(one));
        assert(!b_true.equivalent_to(zero));
        assert(!b_false.equivalent_to(null));
        assert(!b_false.equivalent_to(one));
        assert(!b_false.equivalent_to(zero));
        assert(!null.equivalent_to(one));
        assert(!null.equivalent_to(zero));
        assert(!one.equivalent_to(zero));
    }
    // Scenario 18: Stream Semantics (J.3.7) - Strictness Check
    {
        auto s1 = pdf.newStream("test stream");
        auto s2 = pdf.newStream("DIFFERENT_RAW_BYTES");
        auto s3 = pdf.newStream("test stream");
        s2.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        s3.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        assert(!s1.equivalent_to(s2));
        assert(!s2.equivalent_to(s1));
        assert(!s1.equivalent_to(s3));
        assert(!s3.equivalent_to(s1));
        assert(!s2.equivalent_to(s3));
        assert(!s3.equivalent_to(s2));
    }
    // Scenario 19: Dictionary Value Type Mismatch
    {
        auto d1 = "<< /Key 1 >>"_qpdf;
        auto d2 = "<< /Key (1) >>"_qpdf;
        assert(!d1.equivalent_to(d2));
        assert(!d2.equivalent_to(d1));
    }
    // Scenario 20: Mixed Direct vs Indirect Nesting
    {
        assert(
            QPDFObjectHandle::newArray({Integer(7)})
                .equivalent_to(QPDFObjectHandle::newArray({pdf.makeIndirectObject(Integer(7))})));
    }
    // Scenario 21: Dictionary Subset vs Superset
    {
        auto d1 = "<< /A 1 /B 2 >>"_qpdf;
        auto d2 = "<< /A 1 >>"_qpdf;
        assert(!d1.equivalent_to(d2));
        assert(!d2.equivalent_to(d1));
    }
    // Scenario 22: Stream Semantic Decode Equivalence
    {
        auto s1 = pdf.newStream("Hello World");
        auto s2 = pdf.newStream("HELLO WORLD RAW");
        s2.getDict().replaceKey("/Filter", "/FlateDecode"_qpdf);
        s2.getDict().replaceKey("/DecodeParms", Dictionary::empty());
        assert(!s1.equivalent_to(s2));
        assert(!s2.equivalent_to(s1));
    }
    // Scenario 23: Indirect Object Identity Independence
    {
        auto i1 = pdf.makeIndirectObject(Integer(123));
        auto i2 = Integer(123);
        assert(i1.equivalent_to(pdf.makeIndirectObject(Integer(123))));
        assert(i1.equivalent_to(i2));
        assert(i2.equivalent_to(i1));
    }
    // Scenario 24: Deep Recursive Structure (Stack Safety)
    {
        QPDFObjectHandle a1 = "[]"_qpdf;
        QPDFObjectHandle a2 = "[]"_qpdf;
        QPDFObjectHandle cur1 = a1;
        QPDFObjectHandle cur2 = a2;
        for (int i = 0; i < 200; ++i) {
            auto n1 = "[]"_qpdf;
            auto n2 = "[]"_qpdf;
            cur1.appendItem(n1);
            cur2.appendItem(n2);
            cur1 = n1;
            cur2 = n2;
        }
        assert(!a1.equivalent_to(a2));     // Default depth = 10 -> fails
        assert(a1.equivalent_to(a2, 500)); // Explicit depth -> passes
    }
    // Scenario 25: Wide Graph Fan-out
    {
        auto a1 = "[]"_qpdf;
        auto a2 = "[]"_qpdf;
        auto a3 = "[]"_qpdf;
        for (int i = 0; i < 200; ++i) {
            a1.appendItem(Integer(i));
            a2.appendItem(Integer(i));
            a3.appendItem(Integer(i));
        }
        a3.appendItem(Integer(200));
        assert(a1.equivalent_to(a2));
        assert(!a1.equivalent_to(a3));
        assert(!a3.equivalent_to(a1));
    }
    // Scenario 26: Two Self-Referential Arrays
    {
        auto a1 = pdf.makeIndirectObject("[]"_qpdf);
        auto a2 = pdf.makeIndirectObject("[]"_qpdf);
        a1.appendItem(a1);
        a2.appendItem(a2);
        assert(!a1.equivalent_to(a2));
        assert(!a1.equivalent_to(a2)); // Check idempotency
    }
    // Scenario 27: Nested Dictionary Reuse / Shared Indirect Objects
    {
        auto shared_array = pdf.makeIndirectObject("[42 99]"_qpdf);
        auto dict1 = "<< /Unique1 /A >>"_qpdf;
        dict1.replaceKey("/Shared", shared_array);
        auto dict2 = "<< /Unique1 /A >>"_qpdf;
        dict2.replaceKey("/Shared", shared_array);
        auto dict3 = "<< /Unique1 /B >>"_qpdf;
        dict3.replaceKey("/Shared", shared_array);
        assert(dict1.equivalent_to(dict2));
        assert(!dict1.equivalent_to(dict3));
        assert(!dict3.equivalent_to(dict1));
    }
    // Scenario 28: Shared Indirect Leaves Reached via Two Paths
    {
        auto leaf1 = pdf.makeIndirectObject("[1]"_qpdf);
        auto leaf2 = pdf.makeIndirectObject("[2]"_qpdf);
        auto mid1 = pdf.makeIndirectObject(Dictionary::empty());
        mid1.replaceKey("/Leaf1", leaf1);
        mid1.replaceKey("/Leaf2", leaf2);
        auto mid2 = pdf.makeIndirectObject(Dictionary::empty());
        mid2.replaceKey("/Leaf1", leaf1);
        mid2.replaceKey("/Leaf2", leaf2);
        assert(
            QPDFObjectHandle::newArray({mid1, mid2})
                .equivalent_to(QPDFObjectHandle::newArray({mid1, mid2})));
    }
    // Scenario 29: Direct vs Indirect Integer
    {
        assert(Integer(42).equivalent_to(pdf.makeIndirectObject(Integer(42))));
    }
    // Scenario 30: Nested Diamond with Direct & Indirect Objects
    {
        assert(
            QPDFObjectHandle::newArray(
                {pdf.makeIndirectObject("[42]"_qpdf), pdf.makeIndirectObject("[42]"_qpdf)})
                .equivalent_to(
                    QPDFObjectHandle::newArray(
                        {pdf.makeIndirectObject("[42]"_qpdf),
                         pdf.makeIndirectObject("[42]"_qpdf)})));
    }
    // Scenario 31: Image XObjects sharing an SMask
    {
        auto smask = pdf.newStream();
        smask.replaceStreamData(
            "mask data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        auto img1 = pdf.makeIndirectObject(pdf.newStream());
        img1.replaceStreamData(
            "image1 data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        img1.getDict().replaceKey("/SMask", smask);
        auto img2 = pdf.makeIndirectObject(pdf.newStream());
        img2.replaceStreamData(
            "image1 data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        img2.getDict().replaceKey("/SMask", smask);
        assert(img1.equivalent_to(img2));
    }
    // Scenario 32: Image XObjects with two distinct but identical SMasks
    {
        auto smask1 = pdf.newStream();
        smask1.replaceStreamData(
            "mask data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        auto smask2 = pdf.makeIndirectObject(pdf.newStream());
        smask2.replaceStreamData(
            "mask data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        auto img1 = pdf.newStream();
        img1.replaceStreamData(
            "image1 data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        img1.getDict().replaceKey("/SMask", smask1);
        auto img2 = pdf.makeIndirectObject(pdf.newStream());
        img2.replaceStreamData(
            "image1 data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        img2.getDict().replaceKey("/SMask", smask2);
        assert(img1.equivalent_to(img2));
    }
    // Scenario 33: Dictionary Key Equivalence with Value Mismatch (Annex J)
    {
        assert(!"<< /Key 1 >>"_qpdf.equivalent_to("<< /K#65#79 2 >>"_qpdf));
    }
    // Scenario 34: Uninitialized vs. Uninitialized (!obj)
    {
        assert(QPDFObjectHandle().equivalent_to(QPDFObjectHandle()));
    }
    // Scenario 35: Uninitialized vs. PDF Null
    {
        assert(QPDFObjectHandle().equivalent_to(QPDFObjectHandle::newNull()));
    }
    // Scenario 36: Distinct Direct Null Objects
    {
        assert(QPDFObjectHandle::newNull().equivalent_to(QPDFObjectHandle::newNull()));
    }
    // Scenario 37: Distinct Indirect Nulls (Different IDs)
    {
        assert(pdf.newIndirectNull().equivalent_to(pdf.newIndirectNull()));
    }
    // Scenario 38: Broken References to Different Missing Objects
    {
        // Both missing objects resolve to null, so we expect equivalence
        assert(pdf.getObject(999999, 0).equivalent_to(pdf.getObject(888888, 0)));
    }
    // Scenario 39: Uninitialized Handle vs PDF Null
    {
        auto h_valid_null = QPDFObjectHandle::newNull();
        QPDFObjectHandle h_uninit;
        assert(h_uninit.equivalent_to(h_valid_null));
        assert(h_valid_null.equivalent_to(h_uninit));
    }
    // Scenario 40: Recursion Depth Limit (The Stack Protector)
    {
        auto make_deep_array = [](int levels) {
            QPDFObjectHandle root = Integer(1);
            for (int i = 0; i < levels; ++i) {
                QPDFObjectHandle arr = "[]"_qpdf;
                arr.appendItem(root);
                root = arr;
            }
            return root;
        };
        auto h_pass_1 = make_deep_array(500);
        auto h_pass_2 = make_deep_array(500);
        assert(h_pass_1.equivalent_to(h_pass_1));
        assert(!h_pass_1.equivalent_to(h_pass_2));
        assert(h_pass_1.equivalent_to(h_pass_1, 499));
        assert(!h_pass_1.equivalent_to(h_pass_2, 499));
        assert(h_pass_1.equivalent_to(h_pass_1, 500));
        assert(h_pass_1.equivalent_to(h_pass_2, 500));
        assert(h_pass_1.equivalent_to(h_pass_1, 501));
        assert(h_pass_1.equivalent_to(h_pass_2, 501));
        auto h_fail_1 = make_deep_array(501);
        auto h_fail_2 = make_deep_array(501);
        assert(h_fail_1.equivalent_to(h_fail_1, 499));
        assert(!h_fail_1.equivalent_to(h_fail_2, 499));
        assert(h_fail_1.equivalent_to(h_fail_1, 500));
        assert(!h_fail_1.equivalent_to(h_fail_2, 500));
        assert(h_fail_1.equivalent_to(h_fail_1, 501));
        assert(h_fail_1.equivalent_to(h_fail_2, 501));
    }
    // Scenario 41: Sparse Arrays (null_count > 100 triggers sparse representation)
    {
        auto dense1 = "[]"_qpdf;
        auto null = "null"_qpdf;
        // Build a parse string with 101 nulls to trigger the sparse path
        std::string sparse_str = "[";
        for (int i = 0; i < 101; ++i) {
            sparse_str += "null ";
            dense1.appendItem(null);
        }
        sparse_str += "]";
        auto sparse1 = QPDFObjectHandle::parse(sparse_str);
        assert(sparse1.equivalent_to(QPDFObjectHandle::parse(sparse_str)));
        assert(dense1.equivalent_to(sparse1));
        assert(sparse1.equivalent_to(dense1));
        // Mismatch: replace one null with an integer
        std::string sparse_diff = "[";
        for (int i = 0; i < 100; ++i) {
            sparse_diff += "null ";
        }
        sparse_diff += "42]";
        auto sparse3 = QPDFObjectHandle::parse(sparse_diff);
        assert(!sparse1.equivalent_to(sparse3));
        assert(!sparse3.equivalent_to(sparse1));
        assert(!dense1.equivalent_to(sparse3));
        assert(!sparse3.equivalent_to(dense1));
        std::string sparse_with_value = "[";
        for (int i = 0; i < 101; ++i) {
            sparse_with_value += "null ";
        }
        sparse_with_value += "42 ]"; // one non-null element at index 100
        assert(
            QPDFObjectHandle::parse(sparse_with_value)
                .equivalent_to(QPDFObjectHandle::parse(sparse_with_value)));
    }
    // Scenario 42: equivalent_to on ot_reference (post-replaceObject)
    {
        auto obj = pdf.makeIndirectObject(Integer(42));
        auto replacement = Integer(42);
        // Hold a handle to replacement before it becomes ot_reference
        auto stale = replacement;
        pdf.replaceObject(obj.getObjGen(), replacement);
        // stale's underlying QPDFObject is now ot_reference
        assert(stale.raw_type_code() == ::ot_reference);
        assert(!stale.equivalent_to(Integer(42)));
    }
}

void
runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    std::set<int> ignore_filename = {1, 3};

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
        {0, test_0}, {1, test_1}, {3, test_3}};

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

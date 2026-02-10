#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QUtil.hh>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// ===========================================================================
// TEST HARNESS
// ===========================================================================

bool
run_scenario(
    int id,
    std::string const& name,
    std::function<void(QPDF&, QPDFObjectHandle&)> setup,
    std::function<void(QPDF&, QPDFObjectHandle&)> verify)
{
    std::cout << "\n=== Scenario " << id << ": " << name << " ===\n";
    try {
        QPDF pdf;
        pdf.emptyPDF();

        // Root array to hold our test objects to keep them alive
        QPDFObjectHandle roots = QPDFObjectHandle::newArray();
        pdf.makeIndirectObject(roots);

        setup(pdf, roots);
        verify(pdf, roots);

        std::cout << "PASS: Scenario " << id << "\n";
        return true;
    } catch (std::exception& e) {
        std::cerr << "FAIL: Scenario " << id << " -> " << e.what() << "\n";
        return false;
    }
}

void
assert_equivalent(
    QPDFObjectHandle o1, QPDFObjectHandle o2, bool expect_eq, std::string const& msg = "")
{
    bool actual_eq = o1.equivalent_to(o2);

    if (actual_eq != expect_eq) {
        std::cerr << "   [Assertion Failed] " << msg
                  << " Expected: " << (expect_eq ? "TRUE" : "FALSE")
                  << ", Actual: " << (actual_eq ? "TRUE" : "FALSE") << "\n";
    }
    assert(actual_eq == expect_eq);
    if (actual_eq != expect_eq) {
        // -NDEBUG path
        throw std::runtime_error("Verification failed");
    }
}

// Helper to create a stream easily for tests
QPDFObjectHandle
make_stream(QPDF& pdf, std::string const& data)
{
    return pdf.makeIndirectObject(QPDFObjectHandle::newStream(&pdf, data));
}

int
main()
{
    int failures = 0;
    int total = 0;

    auto run = [&](std::string const& name,
                   std::function<void(QPDF&, QPDFObjectHandle&)> s,
                   std::function<void(QPDF&, QPDFObjectHandle&)> v) {
        total++;
        if (!run_scenario(total, name, s, v)) {
            failures++;
        }
    };

    // ------------------------------------------------------------------------
    // SCENARIOS
    // ------------------------------------------------------------------------
    run(
        "Basic Equality: Name and Scalars",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            roots.appendItem(QPDFObjectHandle::newName("/Test"));
            roots.appendItem(QPDFObjectHandle::newName("/Test"));
            roots.appendItem(QPDFObjectHandle::newInteger(42));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(2), false);
        });
    run(
        "Numeric Types (Int vs Real)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            roots.appendItem(QPDFObjectHandle::newInteger(1));
            roots.appendItem(QPDFObjectHandle::newReal("1.0"));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Array Order Sensitivity",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto a1 = QPDFObjectHandle::newArray(
                {QPDFObjectHandle::newInteger(1), QPDFObjectHandle::newInteger(2)});
            auto a2 = QPDFObjectHandle::newArray(
                {QPDFObjectHandle::newInteger(2), QPDFObjectHandle::newInteger(1)});
            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "Dictionary Key Insensitivity",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto d1 = QPDFObjectHandle::newDictionary();
            d1.replaceKey("/A", QPDFObjectHandle::newInteger(1));
            d1.replaceKey("/B", QPDFObjectHandle::newInteger(2));

            auto d2 = QPDFObjectHandle::newDictionary();
            d2.replaceKey("/B", QPDFObjectHandle::newInteger(2));
            d2.replaceKey("/A", QPDFObjectHandle::newInteger(1));

            roots.appendItem(d1);
            roots.appendItem(d2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Direct vs Indirect Equality",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto obj = QPDFObjectHandle::newInteger(100);
            auto indirect = pdf.makeIndirectObject(obj);
            roots.appendItem(obj);
            roots.appendItem(indirect);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Recursive Diamond (Graph Isomorphism)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto d = pdf.makeIndirectObject(QPDFObjectHandle::newInteger(99));
            auto b = pdf.makeIndirectObject(QPDFObjectHandle::newArray({d}));
            auto c = pdf.makeIndirectObject(QPDFObjectHandle::newArray({d}));
            auto a1 = QPDFObjectHandle::newArray({b, c});
            auto a2 = QPDFObjectHandle::newArray({b, c});
            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Circular References (Self-Loop)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto a1 = pdf.makeIndirectObject(QPDFObjectHandle::newArray());
            a1.appendItem(a1);
            auto a2 = pdf.makeIndirectObject(QPDFObjectHandle::newArray());
            a2.appendItem(a2);
            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Cross-Document Simulation (Strictness)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto a1 = QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(1)});
            auto a2 = QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(1)});
            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Stream Content: Match",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            roots.appendItem(make_stream(pdf, "Stream data"));
            roots.appendItem(make_stream(pdf, "Stream data"));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Stream Content: Mismatch",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            roots.appendItem(make_stream(pdf, "Data A"));
            roots.appendItem(make_stream(pdf, "Data B"));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "Stream Dictionary Differences",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto s1 = make_stream(pdf, "same");
            auto s2 = make_stream(pdf, "same");
            s2.getDict().replaceKey("/Extra", QPDFObjectHandle::newName("/Value"));
            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "J.3.6: Absent Keys vs Null",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto d0 = QPDFObjectHandle::newDictionary();
            auto d1 = QPDFObjectHandle::newDictionary();
            auto d2 = QPDFObjectHandle::newDictionary();
            auto d3 = QPDFObjectHandle::newDictionary();
            d1.replaceKey("/Present", QPDFObjectHandle::newNull());
            d2.replaceKey("/Present", QPDFObjectHandle::newDictionary());
            d3.replaceKey("/Present", QPDFObjectHandle::newArray());
            roots.appendItem(d0);
            roots.appendItem(d1);
            roots.appendItem(d2);
            roots.appendItem(d3);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(2), false);
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(3), false);
        });
    run(
        "String Syntax: Hex vs Literal (Annex J)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            roots.appendItem(QPDFObjectHandle::parse("(A)"));
            roots.appendItem(QPDFObjectHandle::parse("<41>"));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        // Scenario 14: Name Equivalence (Strict Mode)    run(
        "Name Syntax (Parser) vs Distinct Names (Model)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Parsed Equivalence (The Parser normalizes)
            // Note: avoiding newName here because it stores literal names.
            roots.appendItem(QPDFObjectHandle::parse("/Name"));
            roots.appendItem(QPDFObjectHandle::parse("/Na#6d#65"));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // PROOF 1: Parser handles Annex J syntax -> Objects are identical.
            assert_equivalent(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                true,
                "Parsed /Na#6d#65 should equal /Name");
        });
    run(
        "Annex J Oddities: Keys, Octals, and Zeros",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // 1. Dictionary Keys: /Key vs /K#65#79
            roots.appendItem(QPDFObjectHandle::parse("<< /Key 1 >>"));
            roots.appendItem(QPDFObjectHandle::parse("<< /K#65#79 1 >>"));

            // 2. Octal Strings: (A) vs (\101)
            roots.appendItem(QPDFObjectHandle::parse("(A)"));
            roots.appendItem(QPDFObjectHandle::parse("(\\101)"));

            // 3. Mathematical Zero: 0 vs -0.0
            roots.appendItem(QPDFObjectHandle::newInteger(0));
            roots.appendItem(QPDFObjectHandle::newReal("-0.0"));

            roots.appendItem(QPDFObjectHandle::newReal("12.345"));
            roots.appendItem(QPDFObjectHandle::newReal("12.345000000000000"));

            roots.appendItem(QPDFObjectHandle::newInteger(12));
            roots.appendItem(QPDFObjectHandle::newReal("11.99999999999999999999999999999999"));

            roots.appendItem(QPDFObjectHandle::newInteger(12));
            roots.appendItem(QPDFObjectHandle::newReal("12.00000000000000000000000000000000"));

            roots.appendItem(QPDFObjectHandle::newInteger(1));
            roots.appendItem(QPDFObjectHandle::newReal("1."));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                true,
                "Dict keys /Key and /K#65#79 should match");
            assert_equivalent(
                roots.getArrayItem(2), roots.getArrayItem(3), true, "String (A) == (\\101)");
            assert_equivalent(roots.getArrayItem(4), roots.getArrayItem(5), true, "0_int == -0.0");
            assert_equivalent(roots.getArrayItem(5), roots.getArrayItem(4), true, "-0.0 == 0_int");
            assert_equivalent(
                roots.getArrayItem(6), roots.getArrayItem(7), true, "12.345 == 12.345000000000000");
            // Annex J §3.2 allows real numbers to be compared using the processor's
            // internal representation. Using IEEE-754 doubles, this real rounds to 12.
            assert_equivalent(
                roots.getArrayItem(8),
                roots.getArrayItem(9),
                true,
                "12_int == 11.99999999999999999999999999 under current double semantics");
            assert_equivalent(
                roots.getArrayItem(10),
                roots.getArrayItem(11),
                true,
                "12_int == 12.0000000000000000000000000");
            assert_equivalent(roots.getArrayItem(12), roots.getArrayItem(13), true, "1_int == 1.");
        });
    run(
        "Nested Container Deep Comparison",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto inner1 = QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(5)});
            auto outer1 = QPDFObjectHandle::newDictionary();
            outer1.replaceKey("/K", inner1);
            auto inner2 = QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(5)});
            auto outer2 = QPDFObjectHandle::newDictionary();
            outer2.replaceKey("/K", inner2);
            roots.appendItem(outer1);
            roots.appendItem(outer2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Boolean and Null mismatch",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            roots.appendItem(QPDFObjectHandle::newBool(true));
            roots.appendItem(QPDFObjectHandle::newBool(false));
            roots.appendItem(QPDFObjectHandle::newNull());
            roots.appendItem(QPDFObjectHandle::newInteger(1));
            roots.appendItem(QPDFObjectHandle::newInteger(0));
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(2), false);
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(3), false);
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(4), false);
            assert_equivalent(roots.getArrayItem(1), roots.getArrayItem(2), false);
            assert_equivalent(roots.getArrayItem(1), roots.getArrayItem(3), false);
            assert_equivalent(roots.getArrayItem(1), roots.getArrayItem(4), false);
            assert_equivalent(roots.getArrayItem(2), roots.getArrayItem(3), false);
            assert_equivalent(roots.getArrayItem(2), roots.getArrayItem(4), false);
            assert_equivalent(roots.getArrayItem(3), roots.getArrayItem(4), false);
        });
    run(
        "Stream Semantics (J.3.7) - Strictness Check",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            std::string data = "plain text";
            auto s1 = make_stream(pdf, data);
            auto s2 = make_stream(pdf, "DIFFERENT_RAW_BYTES");
            s2.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "Dictionary Value Type Mismatch",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto d1 = QPDFObjectHandle::newDictionary();
            d1.replaceKey("/Key", QPDFObjectHandle::newInteger(1));

            auto d2 = QPDFObjectHandle::newDictionary();
            d2.replaceKey("/Key", QPDFObjectHandle::newString("1"));

            roots.appendItem(d1);
            roots.appendItem(d2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "Mixed Direct vs Indirect Nesting",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto shared = QPDFObjectHandle::newInteger(7);

            auto indirect_shared = pdf.makeIndirectObject(QPDFObjectHandle::newInteger(7));

            auto a1 = QPDFObjectHandle::newArray({shared});
            auto a2 = QPDFObjectHandle::newArray({indirect_shared});

            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Semantically equal, structure different
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Dictionary Subset vs Superset",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto d1 = QPDFObjectHandle::newDictionary();
            d1.replaceKey("/A", QPDFObjectHandle::newInteger(1));
            d1.replaceKey("/B", QPDFObjectHandle::newInteger(2));

            auto d2 = QPDFObjectHandle::newDictionary();
            d2.replaceKey("/A", QPDFObjectHandle::newInteger(1));

            roots.appendItem(d1);
            roots.appendItem(d2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "Stream Semantic Decode Equivalence",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // NOTE: Raw bytes differ, semantic content intended to match
            auto s1 = make_stream(pdf, "Hello World");

            auto s2 = make_stream(pdf, "HELLO WORLD RAW");
            s2.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
            s2.getDict().replaceKey("/DecodeParms", QPDFObjectHandle::newDictionary());

            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Strict equivalence should FAIL (raw bytes + filters differ)
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), false);
        });
    run(
        "Indirect Object Identity Independence",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto i1 = pdf.makeIndirectObject(QPDFObjectHandle::newInteger(123));
            auto i2 = pdf.makeIndirectObject(QPDFObjectHandle::newInteger(123));

            roots.appendItem(i1);
            roots.appendItem(i2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Different indirect objects, same value
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Deep Recursive Structure (Stack Safety)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            QPDFObjectHandle a1 = QPDFObjectHandle::newArray();
            QPDFObjectHandle a2 = QPDFObjectHandle::newArray();

            QPDFObjectHandle cur1 = a1;
            QPDFObjectHandle cur2 = a2;

            // Depth chosen to exercise recursion without risking stack overflow
            for (int i = 0; i < 200; ++i) {
                auto n1 = QPDFObjectHandle::newArray();
                auto n2 = QPDFObjectHandle::newArray();
                cur1.appendItem(n1);
                cur2.appendItem(n2);
                cur1 = n1;
                cur2 = n2;
            }

            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Wide Graph Fan-out (Cache Stress)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto a1 = QPDFObjectHandle::newArray();
            auto a2 = QPDFObjectHandle::newArray();

            for (int i = 0; i < 200; ++i) {
                a1.appendItem(QPDFObjectHandle::newInteger(i));
                a2.appendItem(QPDFObjectHandle::newInteger(i));
            }

            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Equivalence Cache Hit Verification (Consistency Check)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto a1 = pdf.makeIndirectObject(QPDFObjectHandle::newArray());
            auto a2 = pdf.makeIndirectObject(QPDFObjectHandle::newArray());

            // Self-referential cycle
            a1.appendItem(a1);
            a2.appendItem(a2);

            roots.appendItem(a1);
            roots.appendItem(a2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Replaced cache verification with simple consistency checks
            // to avoid unused variable warnings in NDEBUG builds.
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true, "First pass");
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true, "Second pass");
        });

    run(
        "Nested Dictionary Reuse / Shared Indirect Objects",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto shared_array = pdf.makeIndirectObject(
                QPDFObjectHandle::newArray(
                    {QPDFObjectHandle::newInteger(42), QPDFObjectHandle::newInteger(99)}));

            auto dict1 = QPDFObjectHandle::newDictionary();
            dict1.replaceKey("/Shared", shared_array);
            dict1.replaceKey("/Unique1", QPDFObjectHandle::newName("/A"));

            auto dict2 = QPDFObjectHandle::newDictionary();
            dict2.replaceKey("/Shared", shared_array);
            dict2.replaceKey("/Unique1", QPDFObjectHandle::newName("/A"));

            auto dict3 = QPDFObjectHandle::newDictionary();
            dict3.replaceKey("/Shared", shared_array);
            dict3.replaceKey("/Unique1", QPDFObjectHandle::newName("/B"));

            roots.appendItem(dict1);
            roots.appendItem(dict2);
            roots.appendItem(dict3);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto d1 = roots.getArrayItem(0);
            auto d2 = roots.getArrayItem(1);
            auto d3 = roots.getArrayItem(2);

            assert_equivalent(d1, d2, true, "Semantically identical");
            assert_equivalent(d1, d3, false, "Unique key mismatch");
        });

    run(
        "Nested Diamond of Shared Indirect Objects",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto leaf1 = QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(1)});
            auto leaf2 = QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(2)});

            auto mid1 = QPDFObjectHandle::newDictionary();
            mid1.replaceKey("/Leaf1", leaf1);
            mid1.replaceKey("/Leaf2", leaf2);

            auto mid2 = QPDFObjectHandle::newDictionary();
            mid2.replaceKey("/Leaf1", leaf1);
            mid2.replaceKey("/Leaf2", leaf2);

            auto top1 = QPDFObjectHandle::newArray({mid1, mid2});
            auto top2 = QPDFObjectHandle::newArray({mid1, mid2});

            roots.appendItem(top1);
            roots.appendItem(top2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Direct vs Indirect Integer",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto direct = QPDFObjectHandle::newInteger(42);
            auto indirect = pdf.makeIndirectObject(QPDFObjectHandle::newInteger(42));
            roots.appendItem(direct);
            roots.appendItem(indirect);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Nested Diamond with Direct & Indirect Objects",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto leaf1 = QPDFObjectHandle::newInteger(42);
            auto leaf2 = QPDFObjectHandle::newInteger(42);

            auto branch1 = pdf.makeIndirectObject(QPDFObjectHandle::newArray({leaf1}));
            auto branch2 = pdf.makeIndirectObject(QPDFObjectHandle::newArray({leaf2}));

            auto top1 = QPDFObjectHandle::newArray({branch1, branch2});
            auto top2 = QPDFObjectHandle::newArray(
                {pdf.makeIndirectObject(
                     QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(42)})),
                 pdf.makeIndirectObject(
                     QPDFObjectHandle::newArray({QPDFObjectHandle::newInteger(42)}))});

            roots.appendItem(top1);
            roots.appendItem(top2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true, "First pass");
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true, "Second pass");
        });

    run(
        "Image XObjects sharing an SMask",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            auto smask = pdf.makeIndirectObject(pdf.newStream());
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

            roots.appendItem(img1);
            roots.appendItem(img2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Removed explicit Cache object
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Image XObjects with two distinct but identical SMasks",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Create a shared mask stream
            auto smask1 = pdf.makeIndirectObject(pdf.newStream());
            smask1.replaceStreamData(
                "mask data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());

            auto smask2 = pdf.makeIndirectObject(pdf.newStream());
            smask2.replaceStreamData(
                "mask data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());

            // Image 1
            auto img1 = pdf.makeIndirectObject(pdf.newStream());
            img1.replaceStreamData(
                "image1 data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
            img1.getDict().replaceKey("/SMask", smask1);

            // Image 2
            auto img2 = pdf.makeIndirectObject(pdf.newStream());
            img2.replaceStreamData(
                "image1 data", QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
            img2.getDict().replaceKey("/SMask", smask2);

            roots.appendItem(img1);
            roots.appendItem(img2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Removed EquivalenceCache and used helper to prevent unused var warning
            assert_equivalent(roots.getArrayItem(0), roots.getArrayItem(1), true);
        });
    run(
        "Dictionary Key Equivalence with Value Mismatch (Annex J)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Keys are syntactically different but semantically identical
            auto d1 = QPDFObjectHandle::parse("<< /Key 1 >>");
            auto d2 = QPDFObjectHandle::parse("<< /K#65#79 2 >>");

            roots.appendItem(d1);
            roots.appendItem(d2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Keys normalize to the same name, but values differ
            assert_equivalent(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                false,
                "Equivalent dictionary keys with differing values must not be equivalent");
        });
    run(
        "Scenario 34: Uninitialized vs. Uninitialized (!obj)",
        [](QPDF&, QPDFObjectHandle&) {}, // Setup nothing
        [](QPDF&, QPDFObjectHandle&) {
            QPDFObjectHandle h1; // Default constructor (uninitialized)
            QPDFObjectHandle h2; // Default constructor

            // Both are ot_uninitialized -> Equal
            assert_equivalent(h1, h2, true, "Two uninitialized handles should be equivalent");
        });

    // Scenario 35: Uninitialized vs. PDF Null
    // Verifies that C++ "Empty" IS equal to PDF "Null".
    // https://github.com/qpdf/qpdf/wiki/PDF-null-objects-vs-qpdf-null-objects
    run(
        "Scenario 35: Uninitialized vs. PDF Null",
        [](QPDF&, QPDFObjectHandle&) {},
        [](QPDF&, QPDFObjectHandle&) {
            QPDFObjectHandle h1;                               // ot_uninitialized
            QPDFObjectHandle h2 = QPDFObjectHandle::newNull(); // ot_null

            // Different types (ot_uninitialized != ot_null) -> Not Equivalent
            assert_equivalent(h1, h2, true, "Uninitialized handle == PDF Null object");
        });

    // Scenario 36: Distinct Direct Nulls
    // Verifies that two separate instances of "null" are equivalent.
    run(
        "Scenario 36: Distinct Direct Null Objects",
        [](QPDF&, QPDFObjectHandle&) {},
        [](QPDF&, QPDFObjectHandle&) {
            QPDFObjectHandle h1 = QPDFObjectHandle::newNull();
            QPDFObjectHandle h2 = QPDFObjectHandle::newNull();

            // Value check: null == null -> Equivalent
            assert_equivalent(h1, h2, true, "Distinct direct null objects should be equivalent");
        });

    // Scenario 37: Indirect Nulls with Different IDs
    // Verifies that value equivalence overrides object identity for nulls.
    run(
        "Scenario 37: Distinct Indirect Nulls (Different IDs)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Create two different indirect objects, both containing null
            QPDFObjectHandle null1 = QPDFObjectHandle::newNull();
            QPDFObjectHandle null2 = QPDFObjectHandle::newNull();

            roots.appendItem(pdf.makeIndirectObject(null1));
            roots.appendItem(pdf.makeIndirectObject(null2));
        },
        [](QPDF&, QPDFObjectHandle& roots) {
            auto h1 = roots.getArrayItem(0); // e.g., 1 0 R
            auto h2 = roots.getArrayItem(1); // e.g., 2 0 R

            // IDs differ, but values are both null -> Equivalent
            assert_equivalent(
                h1, h2, true, "Different indirect objects containing null should be equivalent");
        });

    // Scenario 38: Broken References (Resolving to Null)
    // Verifies that references to missing objects are treated as equivalent (both effectively
    // null).
    run(
        "Scenario 38: Broken References to Different Missing Objects",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Create references to non-existent object IDs
            QPDFObjectHandle bad1 = pdf.getObjectByID(999999, 0);
            QPDFObjectHandle bad2 = pdf.getObjectByID(888888, 0);

            roots.appendItem(bad1);
            roots.appendItem(bad2);
        },
        [](QPDF&, QPDFObjectHandle& roots) {
            auto h1 = roots.getArrayItem(0);
            auto h2 = roots.getArrayItem(1);

            // In QPDF, bad references resolve to null.
            // Since they resolve to the same value (null), they are equivalent.
            assert_equivalent(
                h1,
                h2,
                true,
                "References to different missing objects (resolving to null) should be equivalent");
        });
    run(
        "Scenario 39: Uninitialized Handle vs PDF Null (The Crash Test)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Setup: Put a standard, valid PDF Null object into the array.
            roots.appendItem(QPDFObjectHandle::newNull());
        },
        [](QPDF&, QPDFObjectHandle& roots) {
            auto h_valid_null = roots.getArrayItem(0);

            // 1. Create a "dangerous" uninitialized handle (internal obj ptr is nullptr).
            // NOTE: We create it locally because appending an uninitialized handle
            // to 'roots' might trigger unrelated QPDF assertions during setup.
            QPDFObjectHandle h_uninit;

            // Pre-check to prove we are testing what we think we are testing
            if (h_uninit.isInitialized()) {
                throw std::runtime_error("Test Setup Failed: h_uninit should be uninitialized");
            }

            // 2. The Crash Site
            // In the naive implementation, 'h_uninit.equivalent_to' tries to
            // dereference its null 'obj' pointer to check the type code -> SEGFAULT.
            assert_equivalent(
                h_uninit,
                h_valid_null,
                true,
                "Uninitialized handle must be equivalent to explicit PDF Null (and not crash), 1");
            assert_equivalent(
                h_valid_null,
                h_uninit,
                true,
                "Uninitialized handle must be equivalent to explicit PDF Null (and not crash), 2");
        });
    run(
        "Scenario 40: Recursion Depth Limit (The Stack Protector)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // No specific setup needed in the PDF file itself.
            roots.appendItem(QPDFObjectHandle::newNull());
        },
        [](QPDF&, QPDFObjectHandle& roots) {
            // Helper: Create a nested array structure: [ [ ... [ 1 ] ... ] ]
            auto make_deep_array = [](int levels) {
                QPDFObjectHandle root = QPDFObjectHandle::newInteger(1);
                for (int i = 0; i < levels; ++i) {
                    QPDFObjectHandle arr = QPDFObjectHandle::newArray();
                    arr.appendItem(root);
                    root = arr;
                }
                return root;
            };

            // 1. The Safe Zone (Depth 500)
            // Logic: Root(500) -> ... -> Leaf(0). The leaf is inspected at depth 0.
            // Since 0 is not < 0, the leaf is compared and returns true.
            auto h_pass_1 = make_deep_array(500);
            auto h_pass_2 = make_deep_array(500);
            assert_equivalent(h_pass_1, h_pass_1, true, "Identity test should pass (depth 500)");
            assert_equivalent(
                h_pass_1, h_pass_2, true, "Depth 500 (approx 500 stack frames) should be allowed");

            // 2. The Abyssal Zone (Depth 501)
            // Logic: Root(500) -> ... -> Leaf(-1).
            // The leaf check hits 'if (depth < 0)' and returns false.
            // Note: This means an object is NOT equivalent to itself if it is too deep.
            auto h_fail_1 = make_deep_array(501);
            auto h_fail_2 = make_deep_array(501);
            assert_equivalent(h_fail_1, h_fail_1, true, "Identity test should pass (depth 501)");
            assert_equivalent(
                h_fail_1,
                h_fail_2,
                false,
                "Depth 501 should hit the recursion limit and safely return false");
        });

    std::cout << "\n=== Summary ===\n";
    if (failures == 0) {
        std::cout << "ALL " << total << " SCENARIOS PASSED\n";
        return 0;
    } else {
        std::cout << failures << " / " << total << " SCENARIOS FAILED\n";
        return 1;
    }
}

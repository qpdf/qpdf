#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QUtil.hh>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================================
// TEST HARNESS
// ============================================================================

bool
runScenario(
    const std::string& name,
    std::function<void(QPDF&, QPDFObjectHandle&)> setup,
    std::function<void(QPDF&, QPDFObjectHandle&)> verify)
{
    std::cout << "\n=== Scenario: " << name << " ===\n";
    try {
        QPDF pdf;
        pdf.emptyPDF();

        // Root array to hold our test objects so they don't get garbage collected
        QPDFObjectHandle roots = QPDFObjectHandle::newArray();
        pdf.makeIndirectObject(roots);

        setup(pdf, roots);

        // CALL THE NEW API
        pdf.deduplicateXobjects();

        // Critical Step: Verification
        verify(pdf, roots);

        std::cout << "PASS: " << name << "\n";
        return true;
    } catch (std::exception& e) {
        std::cerr << "FAIL: " << name << " -> " << e.what() << "\n";
        return false;
    }
}

// Helper assertions
void
assertIdsEqual(QPDFObjectHandle o1, QPDFObjectHandle o2, const std::string& msg)
{
    if (o1.getObjectID() != o2.getObjectID()) {
        throw std::runtime_error(
            msg + " (Expected IDs to match, but " + std::to_string(o1.getObjectID()) +
            " != " + std::to_string(o2.getObjectID()) + ")");
    }
}

void
assertIdsNotEqual(QPDFObjectHandle o1, QPDFObjectHandle o2, const std::string& msg)
{
    if (o1.getObjectID() == o2.getObjectID()) {
        throw std::runtime_error(
            msg + " (Expected IDs to be different, but both are " +
            std::to_string(o1.getObjectID()) + ")");
    }
}

// Helper to make a valid XObject Stream
QPDFObjectHandle
makeXObject(QPDF& pdf, const std::string& data) {
    QPDFObjectHandle s = QPDFObjectHandle::newStream(&pdf, data);
    s.getDict().replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    s.getDict().replaceKey("/Subtype", QPDFObjectHandle::newName("/Form")); // Common for XObjects
    return pdf.makeIndirectObject(s);
}

// ============================================================================
// MAIN SCENARIOS
// ============================================================================

int
main()
{
    int failures = 0;

    // ------------------------------------------------------------------------
    // Scenario 1: Simple Flat Deduplication
    // Purpose: Verify basic hash + dictionary matching works for XObjects.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Simple Flat Dedupe",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            QPDFObjectHandle s1 = makeXObject(pdf, "DATA_FLAT");
            QPDFObjectHandle s2 = makeXObject(pdf, "DATA_FLAT");
            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assertIdsEqual(
                roots.getArrayItem(0), roots.getArrayItem(1), "Identical XObjects should have merged");
        });

    // ------------------------------------------------------------------------
    // Scenario 2: Non-XObject Streams (Negative Test)
    // Purpose: Verify that streams WITHOUT /Type /XObject are IGNORED.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Non-XObject Streams (Ignored)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // These are identical streams, but they lack /Type /XObject
            QPDFObjectHandle s1 = pdf.makeIndirectObject(QPDFObjectHandle::newStream(&pdf, "DATA_PLAIN"));
            QPDFObjectHandle s2 = pdf.makeIndirectObject(QPDFObjectHandle::newStream(&pdf, "DATA_PLAIN"));
            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assertIdsNotEqual(
                roots.getArrayItem(0), roots.getArrayItem(1),
                "Plain streams (not XObjects) should NOT merge");
        });

    // ------------------------------------------------------------------------
    // Scenario 3: Transparency Group Safety (Negative Test)
    // Purpose: Verify XObjects with /Group are IGNORED.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Transparency Group Safety",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            QPDFObjectHandle s1 = makeXObject(pdf, "DATA_GROUP");
            s1.getDict().replaceKey("/Group", QPDFObjectHandle::newDictionary()); // Add Group

            QPDFObjectHandle s2 = makeXObject(pdf, "DATA_GROUP");
            s2.getDict().replaceKey("/Group", QPDFObjectHandle::newDictionary()); // Add Identical Group

            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assertIdsNotEqual(
                roots.getArrayItem(0), roots.getArrayItem(1),
                "XObjects with /Group must NOT merge (Safety Guard)");
        });

    // ------------------------------------------------------------------------
    // Scenario 4: Toxic Streams (Filters)
    // Purpose: Verify dedupe works on underlying data despite filters/garbage.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Toxic Streams (Ignored Filters)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Stream 1
            QPDFObjectHandle t1 = makeXObject(pdf, "GARBAGE_DATA");
            t1.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));

            // Stream 2 (Identical data and dict)
            QPDFObjectHandle t2 = makeXObject(pdf, "GARBAGE_DATA");
            t2.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));

            roots.appendItem(t1);
            roots.appendItem(t2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assertIdsEqual(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                "Toxic XObjects with identical bad filters should merge");
        });

    // ------------------------------------------------------------------------
    // Scenario 5: Metadata Mismatch (Negative Test)
    // Purpose: Verify streams with same data but different dicts DO NOT merge.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Metadata Mismatch",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            QPDFObjectHandle m1 = makeXObject(pdf, "SHARED_DATA");
            m1.getDict().replaceKey("/MyTag", QPDFObjectHandle::newName("/A"));

            QPDFObjectHandle m2 = makeXObject(pdf, "SHARED_DATA");
            m2.getDict().replaceKey("/MyTag", QPDFObjectHandle::newName("/B"));

            roots.appendItem(m1);
            roots.appendItem(m2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assertIdsNotEqual(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                "XObjects with different metadata keys must NOT merge");
        });

    // ------------------------------------------------------------------------
    // Scenario 6: Deep Hierarchy (Recursive Merge)
    // Purpose: Verify parents merge if their children (and grand-children) merge.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Deep Hierarchy (Recursion)",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Bottom Level: Identical XObjects
            QPDFObjectHandle leaf1 = makeXObject(pdf, "DEEP_DATA");
            QPDFObjectHandle leaf2 = makeXObject(pdf, "DEEP_DATA");

            // Mid Level: Containers pointing to leaves
            // Note: These are dictionaries, but we put them inside XObjects for the root
            QPDFObjectHandle mid1 = QPDFObjectHandle::newDictionary();
            mid1.replaceKey("/child", leaf1);

            QPDFObjectHandle mid2 = QPDFObjectHandle::newDictionary();
            mid2.replaceKey("/child", leaf2);

            // Top Level: Roots MUST be XObjects to be candidates for merging
            QPDFObjectHandle s1 = makeXObject(pdf, "ROOT_DATA");
            s1.getDict().replaceKey("/Structure", mid1);

            QPDFObjectHandle s2 = makeXObject(pdf, "ROOT_DATA");
            s2.getDict().replaceKey("/Structure", mid2);

            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // 1. The root streams must merge
            assertIdsEqual(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                "Top-level XObjects should have merged via deep recursion");

            // 2. Integrity Check
            QPDFObjectHandle merged = roots.getArrayItem(0);
            QPDFObjectHandle child = merged.getDict().getKey("/Structure");
            QPDFObjectHandle leaf = child.getKey("/child");

            if (!leaf.isIndirect())
                throw std::runtime_error("Integrity lost: child is not indirect");
        });

    // ------------------------------------------------------------------------
    // Scenario 7: Disjoint Isomorphic Cycles
    // Purpose: Verify graph isomorphism logic (VisitedSet) works on separate islands.
    // ------------------------------------------------------------------------
    failures += !runScenario(
        "Disjoint Isomorphic Cycles",
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            // Island A: S1 -> D1 -> S1
            QPDFObjectHandle s1 = makeXObject(pdf, "CYCLIC_DATA");
            QPDFObjectHandle d1 = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
            s1.getDict().replaceKey("/Next", d1);
            d1.replaceKey("/Back", s1);

            // Island B: S2 -> D2 -> S2
            QPDFObjectHandle s2 = makeXObject(pdf, "CYCLIC_DATA");
            QPDFObjectHandle d2 = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
            s2.getDict().replaceKey("/Next", d2);
            d2.replaceKey("/Back", s2);

            roots.appendItem(s1);
            roots.appendItem(s2);
        },
        [](QPDF& pdf, QPDFObjectHandle& roots) {
            assertIdsEqual(
                roots.getArrayItem(0),
                roots.getArrayItem(1),
                "Isomorphic cyclic XObjects failed to merge");
        });

    std::cout << "\n=== Summary ===\n";
    if (failures == 0) {
        std::cout << "All scenarios passed.\n";
    } else {
        std::cout << failures << " scenario(s) failed.\n";
    }

    return failures;
}

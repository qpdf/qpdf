// Fuzzer for PDF object manipulation operations
// Targets object creation, modification, serialization, and tree operations

#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>

#include <cstdlib>
#include <iostream>

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    std::shared_ptr<QPDF> getQpdf();
    void testObjects();
    void testTrees();
    void doChecks();

    Buffer input_buffer;
    Pl_Discard discard;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    input_buffer(const_cast<unsigned char*>(data), size)
{
}

std::shared_ptr<QPDF>
FuzzHelper::getQpdf()
{
    auto is =
        std::shared_ptr<InputSource>(new BufferInputSource("fuzz input", &this->input_buffer));
    auto qpdf = QPDF::create();
    qpdf->setMaxWarnings(200);
    qpdf->processInputSource(is);
    return qpdf;
}

void
FuzzHelper::testObjects()
{
    std::shared_ptr<QPDF> q = getQpdf();

    // Get all objects and test operations on them
    std::cerr << "info: getAllObjects\n";
    auto objects = q->getAllObjects();

    int obj_count = 0;
    for (auto& obj: objects) {
        ++obj_count;
        if (obj_count > 50) {
            break; // Limit to avoid timeouts
        }

        try {
            // Test type checks
            obj.getTypeName();
            obj.getTypeCode();

            // Test JSON serialization
            std::cerr << "info: getJSON for object " << obj_count << '\n';
            obj.getJSON(JSON::LATEST, true).unparse();

            // Test unparse
            std::cerr << "info: unparse\n";
            obj.unparse();
            obj.unparseResolved();

            // Type-specific operations
            if (obj.isDictionary()) {
                std::cerr << "info: dictionary operations\n";
                auto keys = obj.getKeys();
                for (auto const& key: keys) {
                    obj.getKey(key);
                    obj.hasKey(key);
                }
            } else if (obj.isArray()) {
                std::cerr << "info: array operations\n";
                int n = obj.getArrayNItems();
                for (int i = 0; i < n && i < 20; ++i) {
                    obj.getArrayItem(i);
                }
            } else if (obj.isStream()) {
                std::cerr << "info: stream operations\n";
                obj.getDict();

                // Get stream data
                try {
                    obj.pipeStreamData(&discard, true, false, false);
                } catch (...) {
                    // Stream decode errors are expected
                }
            } else if (obj.isString()) {
                std::cerr << "info: string operations\n";
                obj.getStringValue();
                obj.getUTF8Value();
            } else if (obj.isName()) {
                std::cerr << "info: name operations\n";
                obj.getName();
            } else if (obj.isNumber()) {
                std::cerr << "info: number operations\n";
                if (obj.isInteger()) {
                    obj.getIntValue();
                }
                if (obj.isReal()) {
                    obj.getRealValue();
                }
                obj.getNumericValue();
            }

            // Test object references
            if (obj.isIndirect()) {
                std::cerr << "info: indirect object\n";
                obj.getObjGen();
                obj.getObjectID();
                obj.getGeneration();
            }

        } catch (QPDFExc const& e) {
            std::cerr << "object " << obj_count << " QPDFExc: " << e.what() << '\n';
        } catch (std::runtime_error const& e) {
            std::cerr << "object " << obj_count << " runtime_error: " << e.what() << '\n';
        }
    }

    // Test xref operations
    try {
        std::cerr << "info: xref operations\n";
        // Get xref table to exercise xref code
        auto xref_table = q->getXRefTable();
        std::cerr << "info: xref table has " << xref_table.size() << " entries\n";
    } catch (QPDFExc const& e) {
        std::cerr << "xref QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "xref runtime_error: " << e.what() << '\n';
    }

    // Test trailer
    try {
        std::cerr << "info: trailer operations\n";
        auto trailer = q->getTrailer();
        trailer.getKeys();
        trailer.getJSON(JSON::LATEST, true).unparse();
    } catch (QPDFExc const& e) {
        std::cerr << "trailer QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "trailer runtime_error: " << e.what() << '\n';
    }

    // Test root
    try {
        std::cerr << "info: root operations\n";
        auto root = q->getRoot();
        root.getKeys();
    } catch (QPDFExc const& e) {
        std::cerr << "root QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "root runtime_error: " << e.what() << '\n';
    }
}

void
FuzzHelper::testTrees()
{
    std::shared_ptr<QPDF> q = getQpdf();

    // Try to find and test name trees
    try {
        auto root = q->getRoot();
        if (root.hasKey("/Names")) {
            auto names = root.getKey("/Names");
            if (names.isDictionary()) {
                std::cerr << "info: testing name trees\n";

                // Test various name trees if they exist
                for (auto const& key:
                     {"/Dests", "/AP", "/JavaScript", "/Pages", "/Templates", "/IDS", "/URLS",
                      "/EmbeddedFiles"}) {
                    if (names.hasKey(key)) {
                        try {
                            std::cerr << "info: name tree " << key << '\n';
                            QPDFNameTreeObjectHelper nth(names.getKey(key), *q);
                            nth.begin();
                            nth.end();
                            nth.find("test");
                            nth.hasName("test");
                        } catch (...) {
                            // Name tree errors expected
                        }
                    }
                }
            }
        }
    } catch (QPDFExc const& e) {
        std::cerr << "nametree QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "nametree runtime_error: " << e.what() << '\n';
    }

    // Try to find and test number trees (e.g., page labels)
    try {
        auto root = q->getRoot();
        if (root.hasKey("/PageLabels")) {
            std::cerr << "info: testing page labels number tree\n";
            QPDFNumberTreeObjectHelper nth(root.getKey("/PageLabels"), *q);
            nth.begin();
            nth.end();
            nth.find(0);
            nth.find(1);
        }
    } catch (QPDFExc const& e) {
        std::cerr << "numbertree QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "numbertree runtime_error: " << e.what() << '\n';
    }

    // Write output
    try {
        std::cerr << "info: writing output\n";
        QPDFWriter w(*q);
        w.setOutputPipeline(&discard);
        w.setDeterministicID(true);
        w.write();
    } catch (QPDFExc const& e) {
        std::cerr << "write QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "write runtime_error: " << e.what() << '\n';
    }
}

void
FuzzHelper::doChecks()
{
    Pl_DCT::setMemoryLimit(100'000'000);
    Pl_DCT::setScanLimit(50);
    Pl_PNGFilter::setMemoryLimit(1'000'000);
    Pl_RunLength::setMemoryLimit(1'000'000);
    Pl_TIFFPredictor::setMemoryLimit(1'000'000);
    Pl_Flate::memory_limit(200'000);
    Pl_DCT::setThrowOnCorruptData(true);

    std::cerr << "\ninfo: starting testObjects\n";
    testObjects();
    std::cerr << "\ninfo: starting testTrees\n";
    testTrees();
}

void
FuzzHelper::run()
{
    try {
        doChecks();
    } catch (QPDFExc const& e) {
        std::cerr << "QPDFExc: " << e.what() << '\n';
    } catch (std::runtime_error const& e) {
        std::cerr << "runtime_error: " << e.what() << '\n';
    }
}

extern "C" int
LLVMFuzzerTestOneInput(unsigned char const* data, size_t size)
{
#ifndef _WIN32
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}

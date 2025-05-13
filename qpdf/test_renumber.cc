#include <qpdf/Constants.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFXRefEntry.hh>

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

void
usage()
{
    std::cerr << "Usage: test_renumber [OPTION] INPUT.pdf" << '\n'
              << "Option:" << '\n'
              << "  --object-streams=preserve|disable|generate" << '\n'
              << "  --linearize" << '\n'
              << "  --preserve-unreferenced" << '\n';
}

bool
compare(QPDFObjectHandle a, QPDFObjectHandle b)
{
    static std::set<QPDFObjGen> visited;
    if (a.isIndirect()) {
        if (visited.contains(a.getObjGen())) {
            return true;
        }
        visited.insert(a.getObjGen());
    }

    if (a.getTypeCode() != b.getTypeCode()) {
        std::cerr << "different type code" << '\n';
        return false;
    }

    switch (a.getTypeCode()) {
    case ::ot_boolean:
        if (a.getBoolValue() != b.getBoolValue()) {
            std::cerr << "different boolean" << '\n';
            return false;
        }
        break;
    case ::ot_integer:
        if (a.getIntValue() != b.getIntValue()) {
            std::cerr << "different integer" << '\n';
            return false;
        }
        break;
    case ::ot_real:
        if (a.getRealValue() != b.getRealValue()) {
            std::cerr << "different real" << '\n';
            return false;
        }
        break;
    case ::ot_string:
        if (a.getStringValue() != b.getStringValue()) {
            std::cerr << "different string" << '\n';
            return false;
        }
        break;
    case ::ot_name:
        if (a.getName() != b.getName()) {
            std::cerr << "different name" << '\n';
            return false;
        }
        break;
    case ::ot_array:
        {
            std::vector<QPDFObjectHandle> objs_a = a.getArrayAsVector();
            std::vector<QPDFObjectHandle> objs_b = b.getArrayAsVector();
            size_t items = objs_a.size();
            if (items != objs_b.size()) {
                std::cerr << "different array size" << '\n';
                return false;
            }

            for (size_t i = 0; i < items; ++i) {
                if (!compare(objs_a[i], objs_b[i])) {
                    std::cerr << "different array item" << '\n';
                    return false;
                }
            }
        }
        break;
    case ::ot_dictionary:
        {
            std::set<std::string> keys_a = a.getKeys();
            std::set<std::string> keys_b = b.getKeys();
            if (keys_a != keys_b) {
                std::cerr << "different dictionary keys" << '\n';
                return false;
            }

            for (auto const& key: keys_a) {
                if (!compare(a.getKey(key), b.getKey(key))) {
                    std::cerr << "different dictionary item" << '\n';
                    return false;
                }
            }
        }
        break;
    case ::ot_null:
        break;
    case ::ot_stream:
        std::cout << "stream objects are not compared" << '\n';
        break;
    default:
        std::cerr << "unknown object type" << '\n';
        std::exit(2);
    }

    return true;
}

bool
compare_xref_table(std::map<QPDFObjGen, QPDFXRefEntry> a, std::map<QPDFObjGen, QPDFXRefEntry> b)
{
    if (a.size() != b.size()) {
        std::cerr << "different size" << '\n';
        return false;
    }

    for (auto const& iter: a) {
        std::cout << "xref entry for " << iter.first.getObj() << "/" << iter.first.getGen() << '\n';

        if (!b.contains(iter.first)) {
            std::cerr << "not found" << '\n';
            return false;
        }

        QPDFXRefEntry xref_a = iter.second;
        QPDFXRefEntry xref_b = b[iter.first];
        if (xref_a.getType() != xref_b.getType()) {
            std::cerr << "different xref entry type" << '\n';
            return false;
        }

        switch (xref_a.getType()) {
        case 0:
            break;
        case 1:
            if (xref_a.getOffset() != xref_a.getOffset()) {
                std::cerr << "different offset" << '\n';
                return false;
            }
            break;
        case 2:
            if (xref_a.getObjStreamNumber() != xref_a.getObjStreamNumber() ||
                xref_a.getObjStreamIndex() != xref_a.getObjStreamIndex()) {
                std::cerr << "different stream number or index" << '\n';
                return false;
            }
            break;
        default:
            std::cerr << "unknown xref entry type" << '\n';
            std::exit(2);
        }
    }

    return true;
}

int
main(int argc, char* argv[])
{
    if (argc < 2) {
        usage();
        std::exit(2);
    }

    qpdf_object_stream_e mode = qpdf_o_preserve;
    bool blinearize = false;
    bool bpreserve_unreferenced = false;
    std::string filename_input;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            std::string opt = argv[i];
            if (opt == "--object-streams=preserve") {
                mode = qpdf_o_preserve;
            } else if (opt == "--object-streams=disable") {
                mode = qpdf_o_disable;
            } else if (opt == "--object-streams=generate") {
                mode = qpdf_o_generate;
            } else if (opt == "--linearize") {
                blinearize = true;
            } else if (opt == "--preserve-unreferenced") {
                bpreserve_unreferenced = true;
            } else {
                usage();
                std::exit(2);
            }
        } else if (argc == i + 1) {
            filename_input = argv[i];
            break;
        } else {
            usage();
            std::exit(2);
        }
    }

    try {
        QPDF qpdf_in;
        qpdf_in.processFile(filename_input.c_str());
        std::vector<QPDFObjectHandle> objs_in = qpdf_in.getAllObjects();

        QPDFWriter w(qpdf_in);
        w.setOutputMemory();
        w.setObjectStreamMode(mode);
        w.setLinearization(blinearize);
        w.setPreserveUnreferencedObjects(bpreserve_unreferenced);
        w.write();

        std::map<QPDFObjGen, QPDFXRefEntry> xrefs_w = w.getWrittenXRefTable();
        auto buf = w.getBufferSharedPointer();

        QPDF qpdf_ren;
        qpdf_ren.processMemoryFile(
            "renumbered", reinterpret_cast<char*>(buf->getBuffer()), buf->getSize());
        std::map<QPDFObjGen, QPDFXRefEntry> xrefs_ren = qpdf_ren.getXRefTable();

        std::cout << "--- compare between input and renumbered objects ---" << '\n';
        for (auto const& iter: objs_in) {
            QPDFObjGen og_in = iter.getObjGen();
            QPDFObjGen og_ren = w.getRenumberedObjGen(og_in);

            std::cout << "input " << og_in.getObj() << "/" << og_in.getGen() << " -> renumbered "
                      << og_ren.getObj() << "/" << og_ren.getGen() << '\n';

            if (og_ren.getObj() == 0) {
                std::cout << "deleted" << '\n';
                continue;
            }

            if (!compare(iter, qpdf_ren.getObjectByObjGen(og_ren))) {
                std::cerr << "different" << '\n';
                std::exit(2);
            }
        }
        std::cout << "complete" << '\n';

        std::cout << "--- compare between written and reloaded xref tables ---" << '\n';
        if (!compare_xref_table(xrefs_w, xrefs_ren)) {
            std::cerr << "different" << '\n';
            std::exit(2);
        }
        std::cout << "complete" << '\n';

        std::cout << "succeeded" << '\n';
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        std::exit(2);
    }

    return 0;
}

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

void
usage()
{
    std::cerr << "Usage: test_parsedoffset INPUT.pdf" << std::endl;
}

std::string
make_objdesc(qpdf_offset_t offset, QPDFObjectHandle obj)
{
    std::stringstream ss;
    ss << "offset = " << offset << " (0x" << std::hex << offset << std::dec << "), ";

    if (obj.isIndirect()) {
        ss << "indirect " << obj.getObjectID() << "/" << obj.getGeneration() << ", ";
    } else {
        ss << "direct, ";
    }

    ss << obj.getTypeName();

    return ss.str();
}

void
walk(
    size_t stream_number,
    QPDFObjectHandle obj,
    std::vector<std::vector<std::pair<qpdf_offset_t, std::string>>>& result)
{
    qpdf_offset_t offset = obj.getParsedOffset();
    std::pair<qpdf_offset_t, std::string> p = std::make_pair(offset, make_objdesc(offset, obj));

    if (result.size() < stream_number + 1) {
        result.resize(stream_number + 1);
    }
    result[stream_number].push_back(p);

    if (obj.isArray()) {
        for (auto& oh: obj.getArrayAsVector()) {
            if (!oh.isIndirect()) {
                // QPDF::GetAllObjects() enumerates all indirect objects.
                // So only the direct objects are recursed here.
                walk(stream_number, oh, result);
            }
        }
    } else if (obj.isDictionary()) {
        for (auto const& key: obj.getKeys()) {
            QPDFObjectHandle item = obj.getKey(key);
            if (!item.isIndirect()) {
                // QPDF::GetAllObjects() enumerates all indirect objects.
                // So only the direct objects are recursed here.
                walk(stream_number, item, result);
            }
        }
    } else if (obj.isStream()) {
        walk(stream_number, obj.getDict(), result);
    }
}

void
process(std::string fn, std::vector<std::vector<std::pair<qpdf_offset_t, std::string>>>& result)
{
    QPDF qpdf;
    qpdf.processFile(fn.c_str());
    std::map<QPDFObjGen, QPDFXRefEntry> xrefs = qpdf.getXRefTable();

    for (auto const& oh: qpdf.getAllObjects()) {
        if (xrefs.count(oh.getObjGen()) == 0) {
            std::cerr << oh.getObjectID() << "/" << oh.getGeneration()
                      << " is not found in xref table" << std::endl;
            std::exit(2);
        }

        QPDFXRefEntry xref = xrefs[oh.getObjGen()];
        size_t stream_number;

        switch (xref.getType()) {
        case 0:
            std::cerr << oh.getObjectID() << "/" << oh.getGeneration() << " xref entry is free"
                      << std::endl;
            std::exit(2);
        case 1:
            stream_number = 0;
            break;
        case 2:
            stream_number = static_cast<size_t>(xref.getObjStreamNumber());
            break;
        default:
            std::cerr << "unknown xref entry type" << std::endl;
            std::exit(2);
        }

        walk(stream_number, oh, result);
    }
}

int
main(int argc, char* argv[])
{
    if (argc != 2) {
        usage();
        std::exit(2);
    }

    try {
        std::vector<std::vector<std::pair<qpdf_offset_t, std::string>>> table;

        process(argv[1], table);

        for (size_t i = 0; i < table.size(); ++i) {
            if (table[i].size() == 0) {
                continue;
            }

            std::sort(table[i].begin(), table[i].end());
            if (i == 0) {
                std::cout << "--- objects not in streams ---" << std::endl;
            } else {
                std::cout << "--- objects in stream " << i << " ---" << std::endl;
            }

            for (auto const& iter: table[i]) {
                std::cout << iter.second << std::endl;
            }
        }

        std::cout << "succeeded" << std::endl;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::exit(2);
    }

    return 0;
}

#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " actual expected" << '\n'
              << R"(Where "actual" is the actual output and "expected" is the expected)" << '\n'
              << "output of a test, compare the two PDF files. The files are considered" << '\n'
              << "to match if all their objects are identical except that, if a stream is" << '\n'
              << "compressed with FlateDecode, the uncompressed data must match." << '\n'
              << '\n'
              << "If the files match, the output is the expected file. Otherwise, it is" << '\n'
              << "the actual file. Read comments in the code for rationale." << '\n';
    exit(2);
}

void
cleanEncryption(QPDF& q)
{
    auto enc = q.getTrailer().getKey("/Encrypt");
    if (!enc.isDictionary()) {
        return;
    }
    enc.removeKey("/O");
    enc.removeKey("/OE");
    enc.removeKey("/U");
    enc.removeKey("/UE");
    enc.removeKey("/Perms");
}

std::string
compareObjects(std::string const& label, QPDFObjectHandle act, QPDFObjectHandle exp)
{
    if (act.getTypeCode() != exp.getTypeCode()) {
        QTC::TC("compare", "objects with different type");
        return label + ": different types";
    }
    if (act.isStream()) {
        // Disregard stream lengths. The length of stream data is compared later, and we don't care
        // about the length of compressed data as long as the uncompressed data matches.
        auto act_dict = act.getDict();
        auto exp_dict = exp.getDict();
        act_dict.removeKey("/Length");
        exp_dict.removeKey("/Length");
        if (act_dict.unparse() != exp_dict.unparse()) {
            QTC::TC("compare", "different stream dictionaries");
            return label + ": stream dictionaries differ";
        }
        if (act_dict.getKey("/Type").isNameAndEquals("/XRef")) {
            // Cross-reference streams will generally not match, but we have numerous tests that
            // meaningfully ensure that xref streams are correct.
            QTC::TC("compare", "ignore data for xref stream");
            return "";
        }
        auto act_filters = act_dict.getKey("/Filter");
        bool uncompress = false;
        if (act_filters.isName()) {
            act_filters = act_filters.wrapInArray();
        }
        if (act_filters.isArray()) {
            for (auto& filter: act_filters.aitems()) {
                if (filter.isNameAndEquals("/FlateDecode")) {
                    uncompress = true;
                    break;
                }
            }
        }
        std::shared_ptr<Buffer> act_data;
        std::shared_ptr<Buffer> exp_data;
        if (uncompress) {
            QTC::TC("compare", "uncompressing");
            act_data = act.getStreamData();
            exp_data = exp.getStreamData();
        } else {
            QTC::TC("compare", "not uncompressing");
            act_data = act.getRawStreamData();
            exp_data = exp.getRawStreamData();
        }
        if (act_data->getSize() != exp_data->getSize()) {
            QTC::TC("compare", "differing data size", uncompress ? 0 : 1);
            return label + ": stream data size differs";
        }
        auto act_buf = act_data->getBuffer();
        auto exp_buf = exp_data->getBuffer();
        if (memcmp(act_buf, exp_buf, act_data->getSize()) != 0) {
            QTC::TC("compare", "different data", uncompress ? 0 : 1);
            return label + ": stream data differs";
        }
    } else if (act.unparseResolved() != exp.unparseResolved()) {
        QTC::TC("compare", "different non-stream");
        return label + ": object contents differ";
    }
    return "";
}

void
cleanTrailer(QPDFObjectHandle& trailer)
{
    // If the trailer is an object stream, it will have /Length.
    trailer.removeKey("/Length");
    // Disregard the second half of /ID. This doesn't have anything directly to do with zlib, but
    // lots of tests use --deterministic-id, and that is affected. The deterministic ID tests
    // meaningfully exercise that deterministic IDs behave as expected, so for the rest of the
    // tests, it's okay to ignore /ID[1]. If the two halves of /ID are the same, ignore both since
    // this means qpdf completely generated the /ID rather than preserving the first half.
    auto id = trailer.getKey("/ID");
    if (id.isArray() && id.getArrayNItems() == 2) {
        auto id0 = id.getArrayItem(0).unparse();
        auto id1 = id.getArrayItem(1).unparse();
        id.setArrayItem(1, "()"_qpdf);
        if (id0 == id1) {
            id.setArrayItem(0, "()"_qpdf);
        }
    }
}

std::string
compare(char const* actual_filename, char const* expected_filename, char const* password)
{
    QPDF actual;
    actual.processFile(actual_filename, password);
    QPDF expected;
    expected.processFile(expected_filename, password);
    // The motivation behind this program is to compare files in a way that allows for
    // differences in the exact bytes of zlib compression. If all zlib implementations produced
    // exactly the same output, we would just be able to use straight comparison, but since they
    // don't, we use this. As such, we are enforcing a standard of "sameness" that goes beyond
    // showing semantic equivalence. The only difference we are allowing is compressed data.

    auto act_trailer = actual.getTrailer();
    auto exp_trailer = expected.getTrailer();
    cleanTrailer(act_trailer);
    cleanTrailer(exp_trailer);
    auto trailer_diff = compareObjects("trailer", act_trailer, exp_trailer);
    if (!trailer_diff.empty()) {
        QTC::TC("compare", "different trailer");
        return trailer_diff;
    }

    cleanEncryption(actual);
    cleanEncryption(expected);

    auto actual_objects = actual.getAllObjects();
    auto expected_objects = expected.getAllObjects();
    if (actual_objects.size() != expected_objects.size()) {
        // Not exercised in the test suite since the trailers will differ in this case.
        return "different number of objects";
    }
    for (size_t i = 0; i < actual_objects.size(); ++i) {
        auto act = actual_objects[i];
        auto exp = expected_objects[i];
        auto act_og = act.getObjGen();
        auto exp_og = exp.getObjGen();
        if (act_og != exp_og) {
            // not reproduced in the test suite
            return "different object IDs";
        }
        auto ret = compareObjects(act_og.unparse(), act, exp);
        if (!ret.empty()) {
            return ret;
        }
    }
    return "";
}

int
main(int argc, char* argv[])
{
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << whoami << " from qpdf version " << QPDF::QPDFVersion() << '\n';
        exit(0);
    }

    if (argc < 3 || argc > 4) {
        usage();
    }

    bool show_why = QUtil::get_env("QPDF_COMPARE_WHY");
    try {
        char const* to_output;
        char const* actual = argv[1];
        char const* expected = argv[2];
        char const* password{nullptr};
        if (argc == 4) {
            password = argv[3];
        }
        auto difference = compare(actual, expected, password);
        if (difference.empty()) {
            // The files are identical; write the expected file. This way, tests can be written
            // that compare the output of this program to the expected file.
            to_output = expected;
        } else {
            if (show_why) {
                std::cerr << difference << '\n';
                exit(2);
            }
            // The files differ; write the actual file. If it is determined that the actual file
            // is correct because of changes that result in intended differences, this enables
            // the output of this program to replace the expected file in the test suite.
            to_output = actual;
        }
        auto f = QUtil::safe_fopen(to_output, "rb");
        QUtil::FileCloser fc(f);
        QUtil::binary_stdout();
        auto out = std::make_unique<Pl_StdioFile>("stdout", stdout);
        unsigned char buf[2048];
        bool done = false;
        while (!done) {
            size_t len = fread(buf, 1, sizeof(buf), f);
            if (len <= 0) {
                done = true;
            } else {
                out->write(buf, len);
            }
        }
        if (!difference.empty()) {
            exit(2);
        }
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << '\n';
        exit(2);
    }
    return 0;
}

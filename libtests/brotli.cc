#include <qpdf/assert_test.h>

#include <qpdf/qpdf-config.h>
#ifdef BROTLI
# include <qpdf/Pl_Brotli.hh>
#endif
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace qpdf;

// Generated with the reference brotli CLI, without requiring an encoder at
// build or test time. large uses -w 10; large_window uses --large_window=25.
static auto const hello = QUtil::hex_decode("8f088042726f746c69205044462073747265616d0a03");
static auto const empty = QUtil::hex_decode("3f");
static auto const large = QUtil::hex_decode(
    "21fdff07fc1241f15820907b1fa0ffff805f02200e8b0074ef03f4ff1ff04b00c4610180ee7d80feff037e0980382c"
    "00d0bd0f18");
static auto const large_window =
    QUtil::hex_decode("111922000242726f746c69205044462073747265616d0a03");
static auto const png = QUtil::hex_decode("8f0380010101010203030303");
static auto const tiff = QUtil::hex_decode("8f028001010104010103");

#ifdef BROTLI
static std::string
uncompress(std::string const& encoded, size_t chunk)
{
    Pl_Buffer output("output");
    Pl_Brotli decoder("decode", &output);
    decoder.write(nullptr, 0);
    for (size_t offset = 0; offset < encoded.size(); offset += chunk) {
        decoder.write(
            QUtil::unsigned_char_pointer(encoded) + offset,
            std::min(chunk, encoded.size() - offset));
    }
    decoder.finish();
    return output.getString();
}

static void
expect_error(std::string const& encoded, size_t chunk, std::string const& message)
{
    bool caught = false;
    try {
        uncompress(encoded, chunk);
    } catch (std::runtime_error const& e) {
        caught = true;
        assert(std::string(e.what()).find(message) != std::string::npos);
    }
    assert(caught);
}
#endif

int
main(int argc, char* argv[])
{
    QPDF pdf;
    pdf.emptyPDF();
    auto stream = pdf.newStream(hello);
    auto name = QPDFObjectHandle::newName("/BrotliDecode");
    stream.getDict().replaceKey("/Filter", name);
#ifdef BROTLI
    assert(argc == 2);
    QPDF fixture;
    fixture.processFile(argv[1]);
    assert(
        fixture.getRoot().getKey("/Test").getKey("/Value").getStringValue() ==
        "Brotli object stream");
    auto contents = fixture.getRoot()
                        .getKey("/Pages")
                        .getKey("/Kids")
                        .getArrayItem(0)
                        .getKey("/Contents")
                        .getStreamData();
    assert(
        std::string(reinterpret_cast<char*>(contents->getBuffer()), contents->getSize()) ==
        "q 0 0 20 20 re f Q\n");
    assert(fixture.getWarnings().empty());
    std::string expected = "Brotli PDF stream\n";
    for (size_t chunk = 1; chunk <= hello.size(); ++chunk) {
        assert(uncompress(hello, chunk) == expected);
        assert(uncompress(large_window, chunk) == expected);
        assert(uncompress(large, chunk) == std::string(524288, 'A'));
    }
    assert(uncompress(empty, 1).empty());
    assert(stream.getStreamData()->getSize() == expected.size());
    for (size_t length = 0; length < hello.size(); ++length) {
        expect_error(hello.substr(0, length), 1, "Truncated");
    }
    expect_error(std::string(1, '\xff'), 1, "Brotli decoding failed");
    expect_error(hello + "extra", hello.size() + 5, "Trailing data");
    expect_error(hello + "extra", 1, "Trailing data");
    expect_error(hello + hello, hello.size(), "Trailing data");

    auto limit = Pl_Flate::memory_limit();
    Pl_Flate::memory_limit(1);
    expect_error(hello, 1, "allocation failed");
    Pl_Flate::memory_limit(100000);
    expect_error(large, large.size(), "memory limit exceeded");
    Pl_Flate::memory_limit(0);
    assert(uncompress(large, 1) == std::string(524288, 'A'));
    Pl_Flate::memory_limit(limit);

    // Both predictors reuse the existing Flate/LZW implementation.
    for (auto const& [encoded, parms]:
         {std::pair{png, "<< /Predictor 15 /Columns 3 >>"},
          std::pair{tiff, "<< /Predictor 2 /Columns 3 >>"}}) {
        stream.replaceStreamData(encoded, name, QPDFObjectHandle::parse(parms));
        auto decoded = stream.getStreamData();
        assert(decoded->getSize() == 6);
        assert(std::string(reinterpret_cast<char*>(decoded->getBuffer()), 6) == "\1\2\3\4\5\6");
    }

    // Decoding at the generalized level also composes with ASCIIHex.
    stream.replaceStreamData(
        QUtil::hex_encode(hello) + ">",
        QPDFObjectHandle::parse("[ /ASCIIHexDecode /BrotliDecode ]"),
        QPDFObjectHandle::newNull());
    auto decoded = stream.getStreamData(qpdf_dl_generalized);
    assert(
        std::string(reinterpret_cast<char*>(decoded->getBuffer()), decoded->getSize()) == expected);

    // A normal rewrite converts Brotli to Flate, with no public API changes.
    pdf.getRoot().replaceKey("/TestStream", stream);
    QPDFWriter writer(pdf);
    writer.setOutputMemory();
    writer.write();
    auto buffer = writer.getBufferSharedPointer();
    QPDF reread;
    reread.processMemoryFile(
        "rewritten", reinterpret_cast<char*>(buffer->getBuffer()), buffer->getSize());
    auto rewritten = reread.getRoot().getKey("/TestStream");
    assert(rewritten.getDict().getKey("/Filter").isNameAndEquals("/FlateDecode"));
    decoded = rewritten.getStreamData();
    assert(
        std::string(reinterpret_cast<char*>(decoded->getBuffer()), decoded->getSize()) == expected);
#else
    // Without the optional dependency the filter stays unknown, so streams
    // are still copied losslessly, just as with other unsupported filters.
    bool filtering_attempted = true;
    assert(!stream.pipeStreamData(nullptr, &filtering_attempted, 0, qpdf_dl_generalized));
    assert(!filtering_attempted);
    auto raw = stream.getRawStreamData();
    assert(std::string(reinterpret_cast<char*>(raw->getBuffer()), raw->getSize()) == hello);
#endif
    std::cout << "Brotli tests complete\n";
}

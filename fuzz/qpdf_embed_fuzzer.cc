// Fuzzer for embedded file operations
// Targets QPDFEmbeddedFileDocumentHelper and related file handling

#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFEFStreamObjectHelper.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFFileSpecObjectHelper.hh>
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
    void testEmbeddedFiles();
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
FuzzHelper::testEmbeddedFiles()
{
    std::shared_ptr<QPDF> q = getQpdf();
    QPDFEmbeddedFileDocumentHelper efdh(*q);

    // Test hasEmbeddedFiles
    std::cerr << "info: hasEmbeddedFiles\n";
    bool has_files = efdh.hasEmbeddedFiles();

    // Test validate
    std::cerr << "info: validate\n";
    efdh.validate(true);

    if (has_files) {
        // Get all embedded files
        std::cerr << "info: getEmbeddedFiles\n";
        auto files = efdh.getEmbeddedFiles();

        int file_count = 0;
        for (auto const& iter: files) {
            ++file_count;
            if (file_count > 10) {
                // Limit files processed to avoid timeouts
                break;
            }

            try {
                std::string const& name = iter.first;
                auto filespec = iter.second;
                if (!filespec) {
                    continue;
                }

                std::cerr << "info: processing embedded file: " << name << '\n';

                // Test QPDFFileSpecObjectHelper methods
                std::cerr << "info: getFilename\n";
                filespec->getFilename();

                std::cerr << "info: getDescription\n";
                filespec->getDescription();

                std::cerr << "info: getFilenames\n";
                filespec->getFilenames();

                std::cerr << "info: getEmbeddedFileStreams\n";
                filespec->getEmbeddedFileStreams();

                // Get the embedded file stream
                std::cerr << "info: getEmbeddedFileStream\n";
                auto ef_stream = filespec->getEmbeddedFileStream();

                if (!ef_stream.isNull()) {
                    QPDFEFStreamObjectHelper efs(ef_stream);

                    // Test EF stream methods
                    std::cerr << "info: getCreationDate\n";
                    efs.getCreationDate();

                    std::cerr << "info: getModDate\n";
                    efs.getModDate();

                    std::cerr << "info: getSize\n";
                    efs.getSize();

                    std::cerr << "info: getSubtype\n";
                    efs.getSubtype();

                    std::cerr << "info: getChecksum\n";
                    efs.getChecksum();

                    // Get the stream data (limited)
                    std::cerr << "info: getStreamData\n";
                    auto stream_obj = efs.getObjectHandle();
                    if (stream_obj.isStream()) {
                        // Pipe to discard to exercise decompression
                        // pipeStreamData(pipeline, filter, normalize, compress)
                        stream_obj.pipeStreamData(&discard, true, false, false);
                    }
                }

            } catch (QPDFExc const& e) {
                std::cerr << "embedded file QPDFExc: " << e.what() << '\n';
            } catch (std::runtime_error const& e) {
                std::cerr << "embedded file runtime_error: " << e.what() << '\n';
            }
        }

        // Test removeEmbeddedFile on first file if exists
        if (!files.empty()) {
            try {
                std::cerr << "info: removeEmbeddedFile\n";
                std::string first_name = files.begin()->first;
                efdh.removeEmbeddedFile(first_name);
            } catch (QPDFExc const& e) {
                std::cerr << "removeEmbeddedFile QPDFExc: " << e.what() << '\n';
            } catch (std::runtime_error const& e) {
                std::cerr << "removeEmbeddedFile runtime_error: " << e.what() << '\n';
            }
        }
    }

    // Write output to exercise writer with modified document
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

    std::cerr << "\ninfo: starting testEmbeddedFiles\n";
    testEmbeddedFiles();
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

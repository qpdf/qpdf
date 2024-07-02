// This is an example how to use QPDFJob for removing all Javascript and Action in a PDF file

#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <iostream>

int
main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input file> <output file>\n";
        return 1;
    }

    const char* inputFilePath = argv[1];
    const char* outputFilePath = argv[2];

    try {
        QPDFJob j;
        j.config()->inputFile(inputFilePath)->outputFile(outputFilePath)->checkConfiguration();
        auto qpdf_sp = j.createQPDF();
        auto& pdf = *qpdf_sp;

        auto names = pdf.getRoot().getKey("/Names");
        if (names.isDictionary()) {
            auto js = names.getKey("/JavaScript");
            if (js.isDictionary()) {
                auto nested_names = js.getKey("/Names");
                if (nested_names.isArray()) {
                    js.replaceKey("/Names", "null"_qpdf);
                }
                names.replaceKey("/JavaScript", "null"_qpdf);
            }
        }

        for (auto obj: pdf.getAllObjects()) {
            if (obj.isDictionaryOfType("/Action")) {
                auto s = obj.getKey("/S");
                if (s.isName() && s.getName() == "/JavaScript") {
                    obj.replaceKey("/S", "null"_qpdf);
                }

                auto js = obj.getKey("/JS");
                if (js.isString()) {
                    obj.replaceKey("/JS", "null"_qpdf);
                }

                obj.replaceKey("/Action", "null"_qpdf);
                pdf.getRoot().replaceKey("/Action", "null"_qpdf);
            }
        }
        j.writeQPDF(pdf);
    } catch (std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        exit(2);
    }

    return 0;
}

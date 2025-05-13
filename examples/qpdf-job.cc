#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>

#include <iostream>

// This program is a simple demonstration of different ways to use the QPDFJob API.

static char const* whoami = nullptr;

static void
usage()
{
    std::cerr << "Usage: " << whoami << '\n'
              << "This program linearizes the first page of in.pdf to out1.pdf, out2.pdf, and"
              << '\n'
              << " out3.pdf, each demonstrating a different way to use the QPDFJob API" << '\n';
    exit(2);
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 1) {
        usage();
    }

    // The examples below all catch std::exception. Note that QPDFUsage can be caught separately to
    // report on errors in using the API itself. For CLI, this is command-line usage. For JSON or
    // the API, it would be errors from the equivalent invocation.

    // Note that staticId is used for testing only.

    try {
        // Use the config API
        QPDFJob j;
        j.config()
            ->inputFile("in.pdf")
            ->outputFile("out1.pdf")
            ->pages()
            // Prior to qpdf 11.9.0, call ->pageSpec(file, range, password)
            ->file(".")
            ->range("1")
            ->endPages()
            ->linearize()
            ->staticId()           // for testing only
            ->compressStreams("n") // avoid dependency on zlib output
            ->checkConfiguration();
        j.run();
        std::cout << "out1 status: " << j.getExitCode() << '\n';
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << '\n';
        return 2;
    }

    try {
        char const* new_argv[] = {
            whoami,
            "in.pdf",
            "out2.pdf",
            "--linearize",
            "--pages",
            ".",
            "1",
            "--",
            "--static-id",
            "--compress-streams=n", // avoid dependency on zlib output
            nullptr};
        QPDFJob j;
        j.initializeFromArgv(new_argv);
        j.run();
        std::cout << "out2 status: " << j.getExitCode() << '\n';
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << '\n';
        return 2;
    }

    try {
        // Use the JSON API
        QPDFJob j;
        j.initializeFromJson(R"({
  "inputFile": "in.pdf",
  "outputFile": "out3.pdf",
  "staticId": "",
  "linearize": "",
  "compressStreams": "n",
  "pages": [
    {
      "file": ".",
      "range": "1"
    }
  ]
}
)");
        j.run();
        std::cout << "out3 status: " << j.getExitCode() << '\n';
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << '\n';
        return 2;
    }

    return 0;
}

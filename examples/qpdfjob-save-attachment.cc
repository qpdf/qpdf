#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QUtil.hh>

// This example demonstrates how we can redirect where saved output
// goes by calling the default logger's setSave method before running
// something with QPDFJob. See qpdfjob-c-save-attachment.c for an
// implementation that uses the C API.

int
main(int argc, char* argv[])
{
    auto whoami = QUtil::getWhoami(argv[0]);

    if (argc != 4) {
        std::cerr << "Usage: " << whoami << " file attachment-key outfile"
                  << std::endl;
        exit(2);
    }

    char const* filename = argv[1];
    char const* key = argv[2];
    char const* outfilename = argv[3];
    std::string attachment_arg = "--show-attachment=";
    attachment_arg += key;

    char const* j_argv[] = {
        whoami,
        filename,
        attachment_arg.c_str(),
        "--",
        nullptr,
    };

    QUtil::FileCloser fc(QUtil::safe_fopen(outfilename, "wb"));
    auto save = std::make_shared<Pl_StdioFile>("capture", fc.f);
    QPDFLogger::defaultLogger()->setSave(save, false);

    try {
        QPDFJob j;
        j.initializeFromArgv(j_argv);
        j.run();
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << std::endl;
        exit(2);
    }

    std::cout << whoami << ": wrote attachment to " << outfilename << std::endl;
    return 0;
}

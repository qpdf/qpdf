#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstdlib>
#include <iostream>

// This example demonstrates how we can use the QPDFJob createQPDF and writeQPDF methods to add
// custom transformations to the output produced by QPDFJob runs. The example is a full copy of the
// qpdf program modified to always remove all annotations from the final output.

static char const* whoami = nullptr;

static void
usageExit(std::string const& msg)
{
    std::cerr << std::endl
              << whoami << ": " << msg << std::endl
              << std::endl
              << "For help:" << std::endl
              << "  " << whoami << " --help=usage       usage information" << std::endl
              << "  " << whoami << " --help=topic       help on a topic" << std::endl
              << "  " << whoami << " --help=--option    help on an option" << std::endl
              << "  " << whoami << " --help             general help and a topic list" << std::endl
              << std::endl;
    exit(QPDFJob::EXIT_ERROR);
}

int
realmain(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);

    QPDFJob j;
    try {
        // See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.
        j.initializeFromArgv(argv);
        auto qpdf_sp = j.createQPDF();
        auto& pdf = *qpdf_sp;
        for (auto page: pdf.getAllPages()) {
            page.replaceKey("/Annots", "null"_qpdf);
        }
        j.writeQPDF(pdf);
    } catch (QPDFUsage& e) {
        usageExit(e.what());
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << std::endl;
        return QPDFJob::EXIT_ERROR;
    }
    return j.getExitCode();
}

#ifdef WINDOWS_WMAIN

extern "C" int
wmain(int argc, wchar_t* argv[])
{
    return QUtil::call_main_from_wmain(argc, argv, realmain);
}

#else

int
main(int argc, char* argv[])
{
    return realmain(argc, argv);
}

#endif

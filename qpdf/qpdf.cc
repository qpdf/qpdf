#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFUsage.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static char const* whoami = 0;

static void usageExit(std::string const& msg)
{
    std::cerr
        << std::endl
        << whoami << ": " << msg << std::endl
        << std::endl
        << "For help:" << std::endl
        << "  " << whoami << " --help=usage       usage information"
        << std::endl
        << "  " << whoami << " --help=topic       help on a topic"
        << std::endl
        << "  " << whoami << " --help=--option    help on an option"
        << std::endl
        << "  " << whoami << " --help             general help and a topic list"
        << std::endl
        << std::endl;
    exit(QPDFJob::EXIT_ERROR);
}

int realmain(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);

    // Remove prefix added by libtool for consistency during testing.
    if (strncmp(whoami, "lt-", 3) == 0)
    {
        whoami += 3;
    }

    QPDFJob j;
    try
    {
        // See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.
        j.initializeFromArgv(argv);
        j.run();
    }
    catch (QPDFUsage& e)
    {
        usageExit(e.what());
    }
    catch (std::exception& e)
    {
        std::cerr << whoami << ": " << e.what() << std::endl;
        return QPDFJob::EXIT_ERROR;
    }
    return j.getExitCode();
}

#ifdef WINDOWS_WMAIN

extern "C"
int wmain(int argc, wchar_t* argv[])
{
    return QUtil::call_main_from_wmain(argc, argv, realmain);
}

#else

int main(int argc, char* argv[])
{
    return realmain(argc, argv);
}

#endif

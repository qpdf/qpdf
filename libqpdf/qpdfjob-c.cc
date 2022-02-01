#include <qpdf/qpdfjob-c.h>

#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFUsage.hh>

#include <cstdio>
#include <cstring>

int qpdfjob_run_from_argv(int argc, char* argv[])
{
    auto whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);

    QPDFJob j;
    try
    {
        j.initializeFromArgv(argc, argv);
        j.run();
    }
    catch (std::exception& e)
    {
	std::cerr << whoami << ": " << e.what() << std::endl;
        return QPDFJob::EXIT_ERROR;
    }
    return j.getExitCode();
}

#ifndef QPDF_NO_WCHAR_T
int qpdfjob_run_from_wide_argv(int argc, wchar_t* argv[])
{
    return QUtil::call_main_from_wmain(argc, argv, qpdfjob_run_from_argv);
}
#endif // QPDF_NO_WCHAR_T

int qpdfjob_run_from_json(char const* json)
{
    QPDFJob j;
    try
    {
        j.initializeFromJson(json);
        j.run();
    }
    catch (std::exception& e)
    {
	std::cerr << "qpdfjob json: " << e.what() << std::endl;
        return QPDFJob::EXIT_ERROR;
    }
    return j.getExitCode();
}

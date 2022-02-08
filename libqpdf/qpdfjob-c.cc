#include <qpdf/qpdfjob-c.h>

#include <qpdf/QPDFJob.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFUsage.hh>

#include <cstdio>
#include <cstring>

int qpdfjob_run_from_argv(char const* const argv[])
{
    auto whoami_p = QUtil::make_unique_cstr(argv[0]);
    auto whoami = QUtil::getWhoami(whoami_p.get());
    QUtil::setLineBuf(stdout);

    QPDFJob j;
    try
    {
        j.initializeFromArgv(argv);
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
int qpdfjob_run_from_wide_argv(wchar_t const* const argv[])
{
    int argc = 0;
    for (auto k = argv; *k; ++k)
    {
        ++argc;
    }
    return QUtil::call_main_from_wmain(
        argc, argv, [](int, char const* const new_argv[]) {
            return qpdfjob_run_from_argv(new_argv);
        });
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

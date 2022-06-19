#include <qpdf/qpdfjob-c.h>

#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstring>

struct _qpdfjob_handle
{
    _qpdfjob_handle() = default;
    ~_qpdfjob_handle() = default;

    QPDFJob j;
};

qpdfjob_handle
qpdfjob_init()
{
    return new _qpdfjob_handle;
}

void
qpdfjob_cleanup(qpdfjob_handle* j)
{
    delete *j;
    *j = nullptr;
}

static int
wrap_qpdfjob(qpdfjob_handle j, std::function<int(qpdfjob_handle j)> fn)
{
    try {
        return fn(j);
    } catch (std::exception& e) {
        *j->j.getLogger()->getError()
            << j->j.getMessagePrefix() << ": " << e.what() << "\n";
    }
    return QPDFJob::EXIT_ERROR;
}

int
qpdfjob_initialize_from_argv(qpdfjob_handle j, char const* const argv[])
{
    return wrap_qpdfjob(j, [argv](qpdfjob_handle jh) {
        jh->j.initializeFromArgv(argv);
        return 0;
    });
}

#ifndef QPDF_NO_WCHAR_T
int
qpdfjob_initialize_from_wide_argv(qpdfjob_handle j, wchar_t const* const argv[])
{
    int argc = 0;
    for (auto k = argv; *k; ++k) {
        ++argc;
    }
    return QUtil::call_main_from_wmain(
        argc, argv, [j](int, char const* const new_argv[]) {
            return qpdfjob_initialize_from_argv(j, new_argv);
        });
}
#endif // QPDF_NO_WCHAR_T

int
qpdfjob_initialize_from_json(qpdfjob_handle j, char const* json)
{
    return wrap_qpdfjob(j, [json](qpdfjob_handle jh) {
        jh->j.setMessagePrefix("qpdfjob json");
        jh->j.initializeFromJson(json);
        return 0;
    });
}

int
qpdfjob_run(qpdfjob_handle j)
{
    QUtil::setLineBuf(stdout);
    return wrap_qpdfjob(j, [](qpdfjob_handle jh) {
        jh->j.run();
        return jh->j.getExitCode();
    });
}

static int run_with_handle(std::function<int(qpdfjob_handle)> fn)
{
    auto j = qpdfjob_init();
    int status = fn(j);
    if (status == 0) {
        status = qpdfjob_run(j);
    }
    qpdfjob_cleanup(&j);
    return status;
}

int qpdfjob_run_from_argv(char const* const argv[])
{
    return run_with_handle([argv](qpdfjob_handle j) {
        return qpdfjob_initialize_from_argv(j, argv);
    });
}

#ifndef QPDF_NO_WCHAR_T
int
qpdfjob_run_from_wide_argv(wchar_t const* const argv[])
{
    return run_with_handle([argv](qpdfjob_handle j) {
        return qpdfjob_initialize_from_wide_argv(j, argv);
    });
}
#endif /* QPDF_NO_WCHAR_T */

int qpdfjob_run_from_json(char const* json)
{
    return run_with_handle([json](qpdfjob_handle j) {
        return qpdfjob_initialize_from_json(j, json);
    });
}


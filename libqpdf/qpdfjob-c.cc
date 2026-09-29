#include <qpdf/qpdfjob-c.h>

#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/qpdf-c_impl.hh>
#include <qpdf/qpdflogger-c_impl.hh>

#include <cstdio>
#include <cstdlib>
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
        *j->j.getLogger()->getError() << j->j.getMessagePrefix() << ": " << e.what() << "\n";
    }
    return QPDFJob::EXIT_ERROR;
}

void
qpdfjob_set_logger(qpdfjob_handle j, qpdflogger_handle logger)
{
    j->j.setLogger(logger->l);
}

qpdflogger_handle
qpdfjob_get_logger(qpdfjob_handle j)
{
    return new _qpdflogger_handle(j->j.getLogger());
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
    return QUtil::call_main_from_wmain(argc, argv, [j](int, char const* const new_argv[]) {
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

qpdf_data
qpdfjob_create_qpdf(qpdfjob_handle j)
{
    QUtil::setLineBuf(stdout);
    try {
        auto qpdf = j->j.createQPDF();
        return qpdf ? new _qpdf_data(std::move(qpdf)) : nullptr;
    } catch (std::exception& e) {
        *j->j.getLogger()->getError() << j->j.getMessagePrefix() << ": " << e.what() << "\n";
    }
    return nullptr;
}

int
qpdfjob_write_qpdf(qpdfjob_handle j, qpdf_data qpdf)
{
    QUtil::setLineBuf(stdout);
    return wrap_qpdfjob(j, [qpdf](qpdfjob_handle jh) {
        jh->j.writeQPDF(*(qpdf->qpdf));
        return jh->j.getExitCode();
    });
}

int
questpdf_job_get_xmp_metadata(qpdfjob_handle j, qpdf_data qpdf, unsigned char** bufp, size_t* len)
{
    return wrap_qpdfjob(j, [qpdf, bufp, len](qpdfjob_handle) {
        if (!qpdf || !bufp || !len) {
            throw std::logic_error("questpdf_job_get_xmp_metadata called with a null argument");
        }
        *bufp = nullptr;
        *len = 0;

        auto metadata = qpdf->qpdf->getRoot().getKey("/Metadata");
        if (!metadata.isStream()) {
            return 0;
        }

        auto data = metadata.getStreamData(qpdf_dl_all);
        if (data->getSize() == 0) {
            return 0;
        }
        auto result = static_cast<unsigned char*>(malloc(data->getSize()));
        if (!result) {
            throw std::bad_alloc();
        }
        memcpy(result, data->getBuffer(), data->getSize());
        *bufp = result;
        *len = data->getSize();
        return 0;
    });
}

int
questpdf_job_set_xmp_metadata(
    qpdfjob_handle j, qpdf_data qpdf, unsigned char const* buf, size_t len)
{
    return wrap_qpdfjob(j, [qpdf, buf, len](qpdfjob_handle) {
        if (!qpdf) {
            throw std::logic_error("questpdf_job_set_xmp_metadata called with a null document");
        }
        if (!buf || len == 0) {
            throw std::runtime_error("XMP metadata must not be empty");
        }

        auto root = qpdf->qpdf->getRoot();
        auto metadata = root.getKey("/Metadata");
        if (!metadata.isStream()) {
            metadata = qpdf->qpdf->newStream();
            metadata.getDict().replaceKey("/Type", QPDFObjectHandle::newName("/Metadata"));
            metadata.getDict().replaceKey("/Subtype", QPDFObjectHandle::newName("/XML"));
            root.replaceKey("/Metadata", metadata);
        }
        metadata.replaceStreamData(
            std::string(reinterpret_cast<char const*>(buf), len),
            QPDFObjectHandle::newNull(),
            QPDFObjectHandle::newNull());
        return 0;
    });
}

static int
run_with_handle(std::function<int(qpdfjob_handle)> fn)
{
    auto j = qpdfjob_init();
    int status = fn(j);
    if (status == 0) {
        status = qpdfjob_run(j);
    }
    qpdfjob_cleanup(&j);
    return status;
}

int
qpdfjob_run_from_argv(char const* const argv[])
{
    return run_with_handle(
        [argv](qpdfjob_handle j) { return qpdfjob_initialize_from_argv(j, argv); });
}

#ifndef QPDF_NO_WCHAR_T
int
qpdfjob_run_from_wide_argv(wchar_t const* const argv[])
{
    return run_with_handle(
        [argv](qpdfjob_handle j) { return qpdfjob_initialize_from_wide_argv(j, argv); });
}
#endif /* QPDF_NO_WCHAR_T */

int
qpdfjob_run_from_json(char const* json)
{
    return run_with_handle(
        [json](qpdfjob_handle j) { return qpdfjob_initialize_from_json(j, json); });
}

void
qpdfjob_register_progress_reporter(
    qpdfjob_handle j, void (*report_progress)(int percent, void* data), void* data)
{
    j->j.registerProgressReporter(std::bind(report_progress, std::placeholders::_1, data));
}

int
qpdfjob_register_buffer_input(
    qpdfjob_handle j, char const* name, unsigned char const* data, size_t length)
{
    return wrap_qpdfjob(j, [name, data, length](qpdfjob_handle jh) {
        jh->j.registerBufferInput(name ? name : "", data, length);
        return 0;
    });
}

int
qpdfjob_register_buffer_output(
    qpdfjob_handle j, char const* name, qpdfjob_buffer_output_fn_t fn, void* udata)
{
    return wrap_qpdfjob(j, [name, fn, udata](qpdfjob_handle jh) {
        if (fn == nullptr) {
            throw std::runtime_error("qpdfjob_register_buffer_output: fn may not be null");
        }

        std::string identifier{name ? name : ""};
        jh->j.registerBufferOutput(
            identifier, [identifier, fn, udata](unsigned char const* data, size_t length) {
                int code = fn(data, length, udata);
                if (code != 0) {
                    throw std::runtime_error(
                        "buffer output " + identifier + " function returned code " +
                        std::to_string(code));
                }
            });
        return 0;
    });
}

#include <qpdf/qpdflogger-c.h>

#include <qpdf/Pipeline.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFLogger.hh>
#include <functional>
#include <memory>

struct _qpdflogger_handle
{
    _qpdflogger_handle(std::shared_ptr<QPDFLogger> l);
    ~_qpdflogger_handle() = default;

    std::shared_ptr<QPDFLogger> l;
};

namespace
{
    class FunctionPipeline: public Pipeline
    {
      public:
        FunctionPipeline(char const* identifier, qpdf_log_fn_t fn, void* udata);
        virtual ~FunctionPipeline() = default;

        virtual void write(unsigned char const* buf, size_t len) override;
        virtual void finish() override;

      private:
        qpdf_log_fn_t fn;
        void* udata;
    };
}; // namespace

FunctionPipeline::FunctionPipeline(
    char const* identifier, qpdf_log_fn_t fn, void* udata) :
    Pipeline(identifier, nullptr),
    fn(fn),
    udata(udata)
{
}

void
FunctionPipeline::write(unsigned char const* buf, size_t len)
{
    fn(reinterpret_cast<char const*>(buf), QIntC::to_ulong(len), udata);
}

void
FunctionPipeline::finish()
{
    // Nothing needed
}

_qpdflogger_handle::_qpdflogger_handle(std::shared_ptr<QPDFLogger> l) :
    l(l)
{
}

qpdflogger_handle
qpdflogger_default_logger()
{
    return new _qpdflogger_handle(QPDFLogger::defaultLogger());
}

void
qpdflogger_cleanup(qpdflogger_handle* l)
{
    delete *l;
    *l = nullptr;
}

static void
set_log_dest(
    QPDFLogger* l,
    std::function<void(std::shared_ptr<Pipeline>)> method,
    qpdf_log_dest_e dest,
    char const* identifier,
    qpdf_log_fn_t fn,
    void* udata)
{
    switch (dest) {
    case qpdf_log_dest_default:
        method(nullptr);
        break;
    case qpdf_log_dest_stdout:
        method(l->standardOutput());
        break;
    case qpdf_log_dest_stderr:
        method(l->standardError());
        break;
    case qpdf_log_dest_discard:
        method(l->discard());
        break;
    case qpdf_log_dest_custom:
        method(std::make_shared<FunctionPipeline>(identifier, fn, udata));
        break;
    }
}

static void
set_log_dest(
    QPDFLogger* l,
    void (QPDFLogger::*method)(std::shared_ptr<Pipeline>),
    qpdf_log_dest_e dest,
    char const* identifier,
    qpdf_log_fn_t fn,
    void* udata)
{
    set_log_dest(
        l,
        std::bind(std::mem_fn(method), l, std::placeholders::_1),
        dest,
        identifier,
        fn,
        udata);
}

void
qpdflogger_set_info(
    qpdflogger_handle l, qpdf_log_dest_e dest, qpdf_log_fn_t fn, void* udata)
{
    set_log_dest(
        l->l.get(), &QPDFLogger::setInfo, dest, "info logger", fn, udata);
}

void
qpdflogger_set_warn(
    qpdflogger_handle l, qpdf_log_dest_e dest, qpdf_log_fn_t fn, void* udata)
{
    set_log_dest(
        l->l.get(), &QPDFLogger::setWarn, dest, "warn logger", fn, udata);
}

void
qpdflogger_set_error(
    qpdflogger_handle l, qpdf_log_dest_e dest, qpdf_log_fn_t fn, void* udata)
{
    set_log_dest(
        l->l.get(), &QPDFLogger::setError, dest, "error logger", fn, udata);
}

void
qpdflogger_set_save(
    qpdflogger_handle l,
    qpdf_log_dest_e dest,
    qpdf_log_fn_t fn,
    void* udata,
    int only_if_not_set)
{
    auto method = std::bind(
        std::mem_fn(&QPDFLogger::setSave),
        l->l.get(),
        std::placeholders::_1,
        only_if_not_set);
    set_log_dest(l->l.get(), method, dest, "save logger", fn, udata);
}

void
qpdflogger_save_to_standard_output(qpdflogger_handle l, int only_if_not_set)
{
    qpdflogger_set_save(
        l, qpdf_log_dest_stdout, nullptr, nullptr, only_if_not_set);
}

#include <qpdf/qpdflogger-c.h>

#include <qpdf/QPDFLogger.hh>

struct _qpdflogger_handle
{
    _qpdflogger_handle(std::shared_ptr<QPDFLogger> l);
    ~_qpdflogger_handle() = default;

    std::shared_ptr<QPDFLogger> l;
};

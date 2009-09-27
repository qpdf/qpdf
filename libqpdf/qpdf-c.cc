#include <qpdf/qpdf-c.h>

class _qpdf_data
{
};

DLL_EXPORT
qpdf_data qpdf_init()
{
    return new _qpdf_data();
}

DLL_EXPORT
void qpdf_cleanup(qpdf_data* qpdf)
{
    delete *qpdf;
    *qpdf = 0;
}

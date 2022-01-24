#include <qpdf/QPDFJob.hh>

QPDFJob::Config&
QPDFJob::Config::verbose(bool)
{
    o.m->verbose = true;
    return *this;
}

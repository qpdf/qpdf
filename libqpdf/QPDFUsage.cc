#include <qpdf/QPDFUsage.hh>

QPDFUsage::QPDFUsage(std::string const& msg) :
    std::runtime_error(msg)
{
}

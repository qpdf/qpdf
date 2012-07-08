#include <qpdf/QPDF_Reserved.hh>
#include <stdexcept>

QPDF_Reserved::~QPDF_Reserved()
{
}

std::string
QPDF_Reserved::unparse()
{
    throw std::logic_error("attempt to unparse QPDF_Reserved");
    return "";
}

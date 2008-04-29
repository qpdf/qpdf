
#include <qpdf/QPDF_Null.hh>

QPDF_Null::~QPDF_Null()
{
}

std::string
QPDF_Null::unparse()
{
    return "null";
}

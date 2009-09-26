#include <qpdf/QPDF_Integer.hh>

#include <qpdf/QUtil.hh>

QPDF_Integer::QPDF_Integer(int val) :
    val(val)
{
}

QPDF_Integer::~QPDF_Integer()
{
}

std::string
QPDF_Integer::unparse()
{
    return QUtil::int_to_string(this->val);
}

int
QPDF_Integer::getVal() const
{
    return this->val;
}

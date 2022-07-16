#include <qpdf/QPDFObjGen.hh>

#include <qpdf/QUtil.hh>

std::ostream&
operator<<(std::ostream& os, const QPDFObjGen& og)
{
    os << og.obj << "," << og.gen;
    return os;
}

std::string
QPDFObjGen::unparse(char separator) const
{
    return QUtil::int_to_string(this->obj) + separator +
        QUtil::int_to_string(this->gen);
}

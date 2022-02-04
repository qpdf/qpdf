#include <qpdf/QPDFObjGen.hh>

#include <qpdf/QUtil.hh>

QPDFObjGen::QPDFObjGen() :
    obj(0),
    gen(0)
{
}

QPDFObjGen::QPDFObjGen(int o, int g) :
    obj(o),
    gen(g)
{
}

bool
QPDFObjGen::operator<(QPDFObjGen const& rhs) const
{
    return ((this->obj < rhs.obj) ||
	    ((this->obj == rhs.obj) && (this->gen < rhs.gen)));
}

bool
QPDFObjGen::operator==(QPDFObjGen const& rhs) const
{
    return ((this->obj == rhs.obj) && (this->gen == rhs.gen));
}

int
QPDFObjGen::getObj() const
{
    return this->obj;
}

int
QPDFObjGen::getGen() const
{
    return this->gen;
}

std::ostream& operator<<(std::ostream& os, const QPDFObjGen& og)
{
    os << og.obj << "," << og.gen;
    return os;
}

std::string
QPDFObjGen::unparse() const
{
    return QUtil::int_to_string(this->obj) + "," +
        QUtil::int_to_string(this->gen);
}

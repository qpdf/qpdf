#include <qpdf/QPDFObjGen.hh>

QPDFObjGen::QPDFObjGen(int o = 0, int g = 0) :
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

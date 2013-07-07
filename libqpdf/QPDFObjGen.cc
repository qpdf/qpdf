#include <qpdf/QPDFObjGen.hh>

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

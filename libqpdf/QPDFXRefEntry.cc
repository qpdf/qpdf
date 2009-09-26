#include <qpdf/QPDFXRefEntry.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

DLL_EXPORT
QPDFXRefEntry::QPDFXRefEntry() :
    type(0),
    field1(0),
    field2(0)
{
}

DLL_EXPORT
QPDFXRefEntry::QPDFXRefEntry(int type, int field1, int field2) :
    type(type),
    field1(field1),
    field2(field2)
{
    if ((type < 1) || (type > 2))
    {
	throw QPDFExc("invalid xref type " + QUtil::int_to_string(type));
    }
}

DLL_EXPORT
int
QPDFXRefEntry::getType() const
{
    return this->type;
}

DLL_EXPORT
int
QPDFXRefEntry::getOffset() const
{
    if (this->type != 1)
    {
	throw QPDFExc(
	    "getOffset called for xref entry of type != 1");
    }
    return this->field1;
}

DLL_EXPORT
int
QPDFXRefEntry::getObjStreamNumber() const
{
    if (this->type != 2)
    {
	throw QPDFExc(
	    "getObjStreamNumber called for xref entry of type != 2");
    }
    return this->field1;
}

DLL_EXPORT
int
QPDFXRefEntry::getObjStreamIndex() const
{
    if (this->type != 2)
    {
	throw QPDFExc(
	    "getObjStreamIndex called for xref entry of type != 2");
    }
    return this->field2;
}

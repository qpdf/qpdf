#include <qpdf/QPDFXRefEntry.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/Util.hh>

using namespace qpdf;

QPDFXRefEntry::QPDFXRefEntry() = default;

QPDFXRefEntry::QPDFXRefEntry(int type, qpdf_offset_t field1, int field2) :
    type(type),
    field1(field1),
    field2(field2)
{
    util::assertion(type == 1 || type == 2, "invalid xref type " + std::to_string(type));
}

int
QPDFXRefEntry::getType() const
{
    return type;
}

qpdf_offset_t
QPDFXRefEntry::getOffset() const
{
    util::assertion(type == 1, "getOffset called for xref entry of type != 1");
    return this->field1;
}

int
QPDFXRefEntry::getObjStreamNumber() const
{
    util::assertion(type == 2, "getObjStreamNumber called for xref entry of type != 2");
    return QIntC::to_int(field1);
}

int
QPDFXRefEntry::getObjStreamIndex() const
{
    util::assertion(type == 2, "getObjStreamIndex called for xref entry of type != 2");
    return field2;
}

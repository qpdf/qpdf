#include <qpdf/QPDFXRefEntry.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFExc.hh>

QPDFXRefEntry::QPDFXRefEntry() // NOLINT (modernize-use-equals-default)
{
}

QPDFXRefEntry::QPDFXRefEntry(int type, qpdf_offset_t field1, int field2) :
    type(type),
    field1(field1),
    field2(field2)
{
    if ((type < 1) || (type > 2)) {
        throw std::logic_error("invalid xref type " + std::to_string(type));
    }
}

int
QPDFXRefEntry::getType() const
{
    return this->type;
}

qpdf_offset_t
QPDFXRefEntry::getOffset() const
{
    if (this->type != 1) {
        throw std::logic_error("getOffset called for xref entry of type != 1");
    }
    return this->field1;
}

int
QPDFXRefEntry::getObjStreamNumber() const
{
    if (this->type != 2) {
        throw std::logic_error("getObjStreamNumber called for xref entry of type != 2");
    }
    return QIntC::to_int(this->field1);
}

int
QPDFXRefEntry::getObjStreamIndex() const
{
    if (this->type != 2) {
        throw std::logic_error("getObjStreamIndex called for xref entry of type != 2");
    }
    return this->field2;
}

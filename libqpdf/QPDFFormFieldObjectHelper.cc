#include <qpdf/QPDFFormFieldObjectHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>

QPDFFormFieldObjectHelper::Members::~Members()
{
}

QPDFFormFieldObjectHelper::Members::Members()
{
}

QPDFFormFieldObjectHelper::QPDFFormFieldObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members())
{
}

QPDFFormFieldObjectHelper::QPDFFormFieldObjectHelper() :
    QPDFObjectHelper(QPDFObjectHandle::newNull()),
    m(new Members())
{
}

bool
QPDFFormFieldObjectHelper::isNull()
{
    return this->oh.isNull();
}

QPDFFormFieldObjectHelper
QPDFFormFieldObjectHelper::getParent()
{
    return this->oh.getKey("/Parent"); // may be null
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getInheritableFieldValue(std::string const& name)
{
    QPDFObjectHandle node = this->oh;
    QPDFObjectHandle result(node.getKey(name));
    std::set<QPDFObjGen> seen;
    while (result.isNull() && node.hasKey("/Parent"))
    {
        seen.insert(node.getObjGen());
        node = node.getKey("/Parent");
        if (seen.count(node.getObjGen()))
        {
            break;
        }
        result = node.getKey(name);
        if (! result.isNull())
        {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper non-trivial inheritance");
        }
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsString(
    std::string const& name)
{
    QPDFObjectHandle fv = getInheritableFieldValue(name);
    std::string result;
    if (fv.isString())
    {
        result = fv.getUTF8Value();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsName(
    std::string const& name)
{
    QPDFObjectHandle fv = getInheritableFieldValue(name);
    std::string result;
    if (fv.isName())
    {
        result = fv.getName();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getFieldType()
{
    return getInheritableFieldValueAsName("/FT");
}

std::string
QPDFFormFieldObjectHelper::getFullyQualifiedName()
{
    std::string result;
    QPDFObjectHandle node = this->oh;
    std::set<QPDFObjGen> seen;
    while ((! node.isNull()) && (seen.count(node.getObjGen()) == 0))
    {
        if (node.getKey("/T").isString())
        {
            if (! result.empty())
            {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper non-trivial qualified name");
                result = "." + result;
            }
            result = node.getKey("/T").getUTF8Value() + result;
        }
        seen.insert(node.getObjGen());
        node = node.getKey("/Parent");
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getPartialName()
{
    std::string result;
    if (this->oh.getKey("/T").isString())
    {
        result = this->oh.getKey("/T").getUTF8Value();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getAlternativeName()
{
    if (this->oh.getKey("/TU").isString())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper TU present");
        return this->oh.getKey("/TU").getUTF8Value();
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper TU absent");
    return getFullyQualifiedName();
}

std::string
QPDFFormFieldObjectHelper::getMappingName()
{
    if (this->oh.getKey("/TM").isString())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper TM present");
        return this->oh.getKey("/TM").getUTF8Value();
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper TM absent");
    return getAlternativeName();
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getValue()
{
    return getInheritableFieldValue("/V");
}

std::string
QPDFFormFieldObjectHelper::getValueAsString()
{
    return getInheritableFieldValueAsString("/V");
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getDefaultValue()
{
    return getInheritableFieldValue("/DV");
}

std::string
QPDFFormFieldObjectHelper::getDefaultValueAsString()
{
    return getInheritableFieldValueAsString("/DV");
}

std::string
QPDFFormFieldObjectHelper::getDefaultAppearance()
{
    return getInheritableFieldValueAsString("/DA");
}

int
QPDFFormFieldObjectHelper::getQuadding()
{
    int result = 0;
    QPDFObjectHandle fv = getInheritableFieldValue("/Q");
    if (fv.isInteger())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper Q present");
        result = static_cast<int>(fv.getIntValue());
    }
    return result;
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(
    std::string const& key, QPDFObjectHandle value)
{
    this->oh.replaceKey(key, value);
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(
    std::string const& key, std::string const& utf8_value)
{
    this->oh.replaceKey(key, QPDFObjectHandle::newUnicodeString(utf8_value));
}

void
QPDFFormFieldObjectHelper::setV(
    QPDFObjectHandle value, bool need_appearances)
{
    setFieldAttribute("/V", value);
    if (need_appearances)
    {
        QPDF* qpdf = this->oh.getOwningQPDF();
        if (! qpdf)
        {
            throw std::logic_error(
                "QPDFFormFieldObjectHelper::setV called with"
                " need_appearances = true on an object that is"
                " not associated with an owning QPDF");
        }
        QPDFAcroFormDocumentHelper(*qpdf).setNeedAppearances(true);
    }
}

void
QPDFFormFieldObjectHelper::setV(
    std::string const& utf8_value, bool need_appearances)
{
    setV(QPDFObjectHandle::newUnicodeString(utf8_value),
         need_appearances);
}

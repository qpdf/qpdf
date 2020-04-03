#include <qpdf/QPDF_Array.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QIntC.hh>
#include <stdexcept>

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v)
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(SparseOHArray const& items) :
    elements(items)
{
}

QPDF_Array::~QPDF_Array()
{
}

void
QPDF_Array::releaseResolved()
{
    this->elements.releaseResolved();
}

std::string
QPDF_Array::unparse()
{
    std::string result = "[ ";
    size_t size = this->elements.size();
    for (size_t i = 0; i < size; ++i)
    {
	result += this->elements.at(i).unparse();
	result += " ";
    }
    result += "]";
    return result;
}

JSON
QPDF_Array::getJSON()
{
    JSON j = JSON::makeArray();
    size_t size = this->elements.size();
    for (size_t i = 0; i < size; ++i)
    {
        j.addArrayElement(this->elements.at(i).getJSON());
    }
    return j;
}

QPDFObject::object_type_e
QPDF_Array::getTypeCode() const
{
    return QPDFObject::ot_array;
}

char const*
QPDF_Array::getTypeName() const
{
    return "array";
}

void
QPDF_Array::setDescription(QPDF* qpdf, std::string const& description)
{
    this->QPDFObject::setDescription(qpdf, description);
}

int
QPDF_Array::getNItems() const
{
    // This should really return a size_t, but changing it would break
    // a lot of code.
    return QIntC::to_int(this->elements.size());
}

QPDFObjectHandle
QPDF_Array::getItem(int n) const
{
    if ((n < 0) || (n >= QIntC::to_int(elements.size())))
    {
	throw std::logic_error(
	    "INTERNAL ERROR: bounds error accessing QPDF_Array element");
    }
    return this->elements.at(QIntC::to_size(n));
}

void
QPDF_Array::getAsVector(std::vector<QPDFObjectHandle>& v) const
{
    size_t size = this->elements.size();
    for (size_t i = 0; i < size; ++i)
    {
        v.push_back(this->elements.at(i));
    }
}

void
QPDF_Array::setItem(int n, QPDFObjectHandle const& oh)
{
    this->elements.setAt(QIntC::to_size(n), oh);
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    this->elements = SparseOHArray();
    for (std::vector<QPDFObjectHandle>::const_iterator iter = v.begin();
         iter != v.end(); ++iter)
    {
        this->elements.append(*iter);
    }
}

void
QPDF_Array::insertItem(int at, QPDFObjectHandle const& item)
{
    // As special case, also allow insert beyond the end
    if ((at < 0) || (at > QIntC::to_int(this->elements.size())))
    {
	throw std::logic_error(
	    "INTERNAL ERROR: bounds error accessing QPDF_Array element");
    }
    this->elements.insert(QIntC::to_size(at), item);
}

void
QPDF_Array::appendItem(QPDFObjectHandle const& item)
{
    this->elements.append(item);
}

void
QPDF_Array::eraseItem(int at)
{
    this->elements.erase(QIntC::to_size(at));
}

SparseOHArray const&
QPDF_Array::getElementsForShallowCopy() const
{
    return this->elements;
}

void
QPDF_Array::addExplicitElementsToList(std::list<QPDFObjectHandle>& l) const
{
    for (auto iter = this->elements.begin();
         iter != this->elements.end(); ++iter)
    {
        l.push_back((*iter).second);
    }
}

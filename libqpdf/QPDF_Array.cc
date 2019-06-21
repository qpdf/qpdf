#include <qpdf/QPDF_Array.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QIntC.hh>
#include <stdexcept>

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& items) :
    items(items)
{
}

QPDF_Array::~QPDF_Array()
{
}

void
QPDF_Array::releaseResolved()
{
    for (std::vector<QPDFObjectHandle>::iterator iter = this->items.begin();
	 iter != this->items.end(); ++iter)
    {
	QPDFObjectHandle::ReleaseResolver::releaseResolved(*iter);
    }
}

std::string
QPDF_Array::unparse()
{
    std::string result = "[ ";
    for (std::vector<QPDFObjectHandle>::iterator iter = this->items.begin();
	 iter != this->items.end(); ++iter)
    {
	result += (*iter).unparse();
	result += " ";
    }
    result += "]";
    return result;
}

JSON
QPDF_Array::getJSON()
{
    JSON j = JSON::makeArray();
    for (std::vector<QPDFObjectHandle>::iterator iter = this->items.begin();
	 iter != this->items.end(); ++iter)
    {
        j.addArrayElement((*iter).getJSON());
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
    return QIntC::to_int(this->items.size());
}

QPDFObjectHandle
QPDF_Array::getItem(int n) const
{
    if ((n < 0) || (n >= QIntC::to_int(this->items.size())))
    {
	throw std::logic_error(
	    "INTERNAL ERROR: bounds error accessing QPDF_Array element");
    }
    return this->items.at(QIntC::to_size(n));
}

std::vector<QPDFObjectHandle> const&
QPDF_Array::getAsVector() const
{
    return this->items;
}

void
QPDF_Array::setItem(int n, QPDFObjectHandle const& oh)
{
    // Call getItem for bounds checking
    (void) getItem(n);
    this->items.at(QIntC::to_size(n)) = oh;
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& items)
{
    this->items = items;
}

void
QPDF_Array::insertItem(int at, QPDFObjectHandle const& item)
{
    // As special case, also allow insert beyond the end
    if ((at < 0) || (at > QIntC::to_int(this->items.size())))
    {
	throw std::logic_error(
	    "INTERNAL ERROR: bounds error accessing QPDF_Array element");
    }
    this->items.insert(this->items.begin() + at, item);
}

void
QPDF_Array::appendItem(QPDFObjectHandle const& item)
{
    this->items.push_back(item);
}

void
QPDF_Array::eraseItem(int at)
{
    // Call getItem for bounds checking
    (void) getItem(at);
    this->items.erase(this->items.begin() + at);
}

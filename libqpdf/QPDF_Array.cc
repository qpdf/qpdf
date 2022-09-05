#include <qpdf/QPDF_Array.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <stdexcept>

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v) :
    QPDFValue(::ot_array, "array")
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(SparseOHArray const& items) :
    QPDFValue(::ot_array, "array"),
    elements(items)
{
}

std::shared_ptr<QPDFValueProxy>
QPDF_Array::create(std::vector<QPDFObjectHandle> const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFValueProxy>
QPDF_Array::create(SparseOHArray const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFValueProxy>
QPDF_Array::shallowCopy()
{
    return create(elements);
}

std::string
QPDF_Array::unparse()
{
    std::string result = "[ ";
    size_t size = this->elements.size();
    for (size_t i = 0; i < size; ++i) {
        result += this->elements.at(i).unparse();
        result += " ";
    }
    result += "]";
    return result;
}

JSON
QPDF_Array::getJSON(int json_version)
{
    JSON j = JSON::makeArray();
    size_t size = this->elements.size();
    for (size_t i = 0; i < size; ++i) {
        j.addArrayElement(this->elements.at(i).getJSON(json_version));
    }
    return j;
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
    if ((n < 0) || (n >= QIntC::to_int(elements.size()))) {
        throw std::logic_error(
            "INTERNAL ERROR: bounds error accessing QPDF_Array element");
    }
    return this->elements.at(QIntC::to_size(n));
}

void
QPDF_Array::getAsVector(std::vector<QPDFObjectHandle>& v) const
{
    size_t size = this->elements.size();
    for (size_t i = 0; i < size; ++i) {
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
    for (auto const& iter: v) {
        this->elements.append(iter);
    }
}

void
QPDF_Array::insertItem(int at, QPDFObjectHandle const& item)
{
    // As special case, also allow insert beyond the end
    if ((at < 0) || (at > QIntC::to_int(this->elements.size()))) {
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

void
QPDF_Array::addExplicitElementsToList(std::list<QPDFObjectHandle>& l) const
{
    for (auto const& iter: this->elements) {
        l.push_back(iter.second);
    }
}

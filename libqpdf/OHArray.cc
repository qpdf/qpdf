#include <qpdf/OHArray.hh>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>

#include <stdexcept>

OHArray::OHArray() :
    n_elements(0)
{
}

size_t
OHArray::size() const
{
    return this->n_elements;
}

void
OHArray::append(QPDFObjectHandle oh)
{
    if (!oh.isDirectNull()) {
        this->elements[this->n_elements] = oh;
    }
    ++this->n_elements;
}

void
OHArray::append(std::shared_ptr<QPDFObject>&& obj)
{
    if (obj->getTypeCode() != ::ot_null || !obj->getObjGen().isIndirect()) {
        this->elements[this->n_elements] = std::move(obj);
    }
    ++this->n_elements;
}

QPDFObjectHandle
OHArray::at(size_t idx) const
{
    if (idx >= this->n_elements) {
        throw std::logic_error(
            "INTERNAL ERROR: bounds error accessing OHArray element");
    }
    auto const& iter = this->elements.find(idx);
    if (iter == this->elements.end()) {
        return QPDFObjectHandle::newNull();
    } else {
        return (*iter).second;
    }
}

void
OHArray::remove_last()
{
    if (this->n_elements == 0) {
        throw std::logic_error("INTERNAL ERROR: attempt to remove"
                               " last item from empty OHArray");
    }
    --this->n_elements;
    this->elements.erase(this->n_elements);
}

void
OHArray::disconnect()
{
    for (auto& iter: this->elements) {
        QPDFObjectHandle::DisconnectAccess::disconnect(iter.second);
    }
}

void
OHArray::setAt(size_t idx, QPDFObjectHandle oh)
{
    if (idx >= this->n_elements) {
        throw std::logic_error("bounds error setting item in OHArray");
    }
    if (oh.isDirectNull()) {
        this->elements.erase(idx);
    } else {
        this->elements[idx] = oh;
    }
}

void
OHArray::erase(size_t idx)
{
    if (idx >= this->n_elements) {
        throw std::logic_error("bounds error erasing item from OHArray");
    }
    decltype(this->elements) dest;
    for (auto const& iter: this->elements) {
        if (iter.first < idx) {
            dest.insert(iter);
        } else if (iter.first > idx) {
            dest[iter.first - 1] = iter.second;
        }
    }
    this->elements = dest;
    --this->n_elements;
}

void
OHArray::insert(size_t idx, QPDFObjectHandle oh)
{
    if (idx > this->n_elements) {
        throw std::logic_error("bounds error inserting item to OHArray");
    } else if (idx == this->n_elements) {
        // Allow inserting to the last position
        append(oh);
    } else {
        decltype(this->elements) dest;
        for (auto const& iter: this->elements) {
            if (iter.first < idx) {
                dest.insert(iter);
            } else {
                dest[iter.first + 1] = iter.second;
            }
        }
        this->elements = dest;
        this->elements[idx] = oh;
        ++this->n_elements;
    }
}

OHArray
OHArray::copy()
{
    OHArray result;
    result.n_elements = this->n_elements;
    for (auto const& element: this->elements) {
        auto value = element.second;
        result.elements[element.first] =
            value.isIndirect() ? value : value.shallowCopy();
    }
    return result;
}

OHArray::const_iterator
OHArray::begin() const
{
    return this->elements.begin();
}

OHArray::const_iterator
OHArray::end() const
{
    return this->elements.end();
}

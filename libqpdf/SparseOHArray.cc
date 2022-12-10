#include <qpdf/SparseOHArray.hh>

#include <stdexcept>

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

QPDFObjectHandle
SparseOHArray::at(int idx) const
{
    auto const& iter = elements.find(idx);
    return iter == elements.end() ? null_oh : (*iter).second;
}

void
SparseOHArray::remove_last()
{
    if (this->n_elements == 0) {
        throw std::logic_error("INTERNAL ERROR: attempt to remove"
                               " last item from empty SparseOHArray");
    }
    --this->n_elements;
    this->elements.erase(this->n_elements);
}

void
SparseOHArray::disconnect()
{
    for (auto& iter: this->elements) {
        QPDFObjectHandle::DisconnectAccess::disconnect(iter.second);
    }
}

void
SparseOHArray::setAt(int idx, QPDFObjectHandle oh)
{
    if (idx >= this->n_elements) {
        throw std::logic_error("bounds error setting item in SparseOHArray");
    }
    if (oh.isDirectNull()) {
        this->elements.erase(idx);
    } else {
        this->elements[idx] = oh.getObj();
    }
}

void
SparseOHArray::erase(int idx)
{
    if (idx >= this->n_elements) {
        throw std::logic_error("bounds error erasing item from SparseOHArray");
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
SparseOHArray::insert(int idx, QPDFObjectHandle oh)
{
    if (idx > this->n_elements) {
        throw std::logic_error("bounds error inserting item to SparseOHArray");
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
        this->elements[idx] = oh.getObj();
        ++this->n_elements;
    }
}

SparseOHArray
SparseOHArray::copy()
{
    SparseOHArray result;
    result.n_elements = this->n_elements;
    for (auto const& element: this->elements) {
        auto value = element.second;
        result.elements[element.first] =
            value->getObjGen().isIndirect() ? value : value->copy();
    }
    return result;
}

SparseOHArray::const_iterator
SparseOHArray::begin() const
{
    return this->elements.begin();
}

SparseOHArray::const_iterator
SparseOHArray::end() const
{
    return this->elements.end();
}

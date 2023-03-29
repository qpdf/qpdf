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

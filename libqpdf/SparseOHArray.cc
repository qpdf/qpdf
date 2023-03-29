#include <qpdf/SparseOHArray.hh>

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

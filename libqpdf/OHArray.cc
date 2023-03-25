#include <qpdf/OHArray.hh>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>

#include <stdexcept>

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

OHArray::OHArray()
{
}

QPDFObjectHandle
OHArray::at(size_t idx) const
{
    if (idx >= elements.size()) {
        throw std::logic_error(
            "INTERNAL ERROR: bounds error accessing OHArray element");
    }
    auto const& obj = elements.at(idx);
    return obj ? obj : null_oh;
}

void
OHArray::disconnect()
{
    for (auto const& iter: elements) {
        if (iter) {
            QPDFObjectHandle oh = iter;
            QPDFObjectHandle::DisconnectAccess::disconnect(oh);
        }
    }
}

void
OHArray::setAt(size_t idx, QPDFObjectHandle oh)
{
    if (idx >= elements.size()) {
        throw std::logic_error("bounds error setting item in OHArray");
    }
    elements[idx] = oh.getObj();
}

void
OHArray::erase(size_t idx)
{
    if (idx >= elements.size()) {
        throw std::logic_error("bounds error erasing item from OHArray");
    }
    int n = int(idx);
    elements.erase(elements.cbegin() + n);
}

void
OHArray::insert(size_t idx, QPDFObjectHandle oh)
{
    if (idx > elements.size()) {
        throw std::logic_error("bounds error inserting item to OHArray");
    } else if (idx == elements.size()) {
        // Allow inserting to the last position
        elements.push_back(oh.getObj());
    } else {
        int n = int(idx);
        elements.insert(elements.cbegin() + n, oh.getObj());
    }
}

OHArray
OHArray::copy()
{
    OHArray result;
    result.elements.reserve(elements.size());
    for (auto const& element: elements) {
        result.elements.push_back(
            element ? (element->getObjGen().isIndirect() ? element
                                                         : element->copy())
                    : element);
    }
    return result;
}

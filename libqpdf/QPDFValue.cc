#include <qpdf/QPDFValue.hh>

#include <qpdf/QPDFObject_private.hh>

std::shared_ptr<QPDFObject>
QPDFValue::do_create(QPDFValue* object)
{
    std::shared_ptr<QPDFObject> obj(new QPDFObject());
    obj->value = std::shared_ptr<QPDFValue>(object);
    return obj;
}

void
QPDFValue::checkOwnership(QPDFObject* const item) const
{
    auto item_qpdf = item ? item->getQPDF() : nullptr;
    if ((qpdf != nullptr) && (item_qpdf != nullptr) && (qpdf != item_qpdf)) {
        // QTC::TC("qpdf", "QPDFValue check ownership");
        throw std::logic_error(
            "Attempting to add an object from a different QPDF. Use "
            "QPDF::copyForeignObject to add objects from another file.");
    }
}

// Array methods

int
QPDFValue::size() const
{
    return 1;
}

QPDFObjectHandle
QPDFValue::at(int) const
{
    return {};
}

bool
QPDFValue::setAt(int, QPDFObjectHandle const&)
{
    return false;
}

bool
QPDFValue::erase(int at)
{
    return false;
}

bool
QPDFValue::insert(int, QPDFObjectHandle const&)
{
    return false;
}

bool
QPDFValue::push_back(QPDFObjectHandle const& item)
{
    return false;
}

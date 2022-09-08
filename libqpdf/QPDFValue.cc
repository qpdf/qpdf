#include <qpdf/QPDFValue.hh>

#include <qpdf/QPDFObject_private.hh>

std::shared_ptr<QPDFObject>
QPDFValue::do_create(QPDFValue* object)
{
    std::shared_ptr<QPDFObject> obj(new QPDFObject());
    obj->value = std::shared_ptr<QPDFValue>(object);
    return obj;
}

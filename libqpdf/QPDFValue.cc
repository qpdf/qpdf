#include <qpdf/QPDFValue.hh>

#include <qpdf/QPDFObject_private.hh>

std::shared_ptr<QPDFValueProxy>
QPDFValue::do_create(QPDFValue* object)
{
    std::shared_ptr<QPDFValueProxy> obj(new QPDFValueProxy());
    obj->value = std::shared_ptr<QPDFValue>(object);
    return obj;
}

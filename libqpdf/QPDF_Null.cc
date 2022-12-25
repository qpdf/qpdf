#include <qpdf/QPDF_Null.hh>

#include <qpdf/QPDFObject_private.hh>

QPDF_Null::QPDF_Null() :
    QPDFValue(::ot_null, "null")
{
}

std::shared_ptr<QPDFObject>
QPDF_Null::create()
{
    return do_create(new QPDF_Null());
}

std::shared_ptr<QPDFObject>
QPDF_Null::create(
    std::shared_ptr<QPDFObject> parent,
    std::string_view const& static_descr,
    std::string var_descr)
{
    auto n = do_create(new QPDF_Null());
    n->setChildDescription(parent, static_descr, var_descr);
    return n;
}

std::shared_ptr<QPDFObject>
QPDF_Null::create(
    std::shared_ptr<QPDFValue> parent,
    std::string_view const& static_descr,
    std::string var_descr)
{
    auto n = do_create(new QPDF_Null());
    n->setChildDescription(parent, static_descr, var_descr);
    return n;
}

std::shared_ptr<QPDFObject>
QPDF_Null::copy(bool shallow)
{
    return create();
}

std::string
QPDF_Null::unparse()
{
    return "null";
}

JSON
QPDF_Null::getJSON(int json_version)
{
    // If this is updated, QPDF_Array::getJSON must also be updated.
    return JSON::makeNull();
}

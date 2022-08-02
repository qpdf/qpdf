#include <qpdf/QPDF_Unresolved.hh>

#include <stdexcept>

std::shared_ptr<QPDFObject>
QPDF_Unresolved::create()
{
    return do_create(new QPDF_Unresolved());
}

std::shared_ptr<QPDFObject>
QPDF_Unresolved::shallowCopy()
{
    return create();
}

std::string
QPDF_Unresolved::unparse()
{
    throw std::logic_error(
        "attempted to unparse an unresolved QPDFObjectHandle");
    return "";
}

JSON
QPDF_Unresolved::getJSON(int json_version)
{
    return JSON::makeNull();
}

qpdf_object_type_e
QPDF_Unresolved::getTypeCode() const
{
    return ::ot_unresolved;
}

char const*
QPDF_Unresolved::getTypeName() const
{
    return "unresolved";
}

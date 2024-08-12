#include <qpdf/QPDF_Null.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QPDFObject_private.hh>

QPDF_Null::QPDF_Null(QPDF* qpdf, QPDFObjGen og) :
    QPDFValue(::ot_null, qpdf, og)
{
}

std::shared_ptr<QPDFObject>
QPDF_Null::create(QPDF* qpdf, QPDFObjGen og)
{
    return do_create(new QPDF_Null(qpdf, og));
}

std::shared_ptr<QPDFObject>
QPDF_Null::create(
    std::shared_ptr<QPDFObject> parent, std::string_view const& static_descr, std::string var_descr)
{
    auto n = do_create(new QPDF_Null());
    n->setChildDescription(parent, static_descr, var_descr);
    return n;
}

std::shared_ptr<QPDFObject>
QPDF_Null::create(
    std::shared_ptr<QPDFValue> parent, std::string_view const& static_descr, std::string var_descr)
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

void
QPDF_Null::writeJSON(int json_version, JSON::Writer& p)
{
    p << "null";
}

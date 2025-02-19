#include <qpdf/QPDF_Unresolved.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObject_private.hh>

QPDF_Unresolved::QPDF_Unresolved(QPDF* qpdf, QPDFObjGen og) :
    QPDFValue(::ot_unresolved, qpdf, og)
{
}

std::shared_ptr<QPDFObject>
QPDF_Unresolved::create(QPDF* qpdf, QPDFObjGen og)
{
    return do_create(new QPDF_Unresolved(qpdf, og));
}

std::shared_ptr<QPDFObject>
QPDF_Unresolved::copy(bool shallow)
{
    return QPDF::Resolver::resolved(qpdf, og)->copy(shallow);
}

std::string
QPDF_Unresolved::unparse()
{
    return QPDF::Resolver::resolved(qpdf, og)->unparse();
}

void
QPDF_Unresolved::writeJSON(int json_version, JSON::Writer& p)
{
    QPDF::Resolver::resolved(qpdf, og)->writeJSON(json_version, p);
}

std::string
QPDF_Unresolved::getStringValue() const
{
    return QPDF::Resolver::resolved(qpdf, og)->getStringValue();
}

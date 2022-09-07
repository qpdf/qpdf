#ifndef QPDFVALUEPROXY_HH
#define QPDFVALUEPROXY_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/Types.h>

#include <string>

class QPDF;
class QPDFObjectHandle;

class QPDFValueProxy
{
    friend class QPDFValue;

  public:
    QPDFValueProxy() = default;

    std::shared_ptr<QPDFValueProxy>
    shallowCopy()
    {
        return value->shallowCopy();
    }
    std::string
    unparse()
    {
        return value->unparse();
    }
    JSON
    getJSON(int json_version)
    {
        return value->getJSON(json_version);
    }

    // Return a unique type code for the object
    qpdf_object_type_e
    getTypeCode() const
    {
        return value->type_code;
    }

    // Return a string literal that describes the type, useful for
    // debugging and testing
    char const*
    getTypeName() const
    {
        return value->type_name;
    }

    QPDF*
    getQPDF() const
    {
        return value->qpdf;
    }
    QPDFObjGen
    getObjGen() const
    {
        return value->og;
    }

    void
    setDescription(QPDF* qpdf, std::string const& description)
    {
        return value->setDescription(qpdf, description);
    }
    bool
    getDescription(QPDF*& qpdf, std::string& description)
    {
        return value->getDescription(qpdf, description);
    }
    bool
    hasDescription()
    {
        return value->hasDescription();
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        value->setParsedOffset(offset);
    }
    qpdf_offset_t
    getParsedOffset()
    {
        return value->getParsedOffset();
    }
    void
    assign(std::shared_ptr<QPDFValueProxy> o)
    {
        value = o->value;
    }
    void
    swapWith(std::shared_ptr<QPDFValueProxy> o)
    {
        auto v = value;
        value = o->value;
        o->value = v;
        auto og = value->og;
        value->og = o->value->og;
        o->value->og = og;
    }

    // The following two methods are for use by class QPDF only
    void
    setObjGen(QPDF* qpdf, QPDFObjGen const& og)
    {
        value->qpdf = qpdf;
        value->og = og;
    }
    void
    reset()
    {
        value->reset();
        // It would be better if, rather than clearing value->qpdf and
        // value->og, we completely replaced value with a null object.
        // However, at the time of the release of qpdf 11, this causes
        // test failures and would likely break a lot of code since it
        // possible for a direct object that recursively contains no
        // indirect objects to be copied into multiple QPDF objects.
        // For that reason, we have to break the association with the
        // owning QPDF but not otherwise mutate the object. For
        // indirect objects, QPDF::~QPDF replaces the object with
        // null, which clears circular references. If this code were
        // able to do the null replacement, that code would not have
        // to.
        value->qpdf = nullptr;
        value->og = QPDFObjGen();
    }

    bool
    isUnresolved() const
    {
        return value->type_code == ::ot_unresolved;
    }
    void
    resolve()
    {
        if (isUnresolved()) {
            doResolve();
        }
    }
    void doResolve();

    template <typename T>
    T*
    as()
    {
        return dynamic_cast<T*>(value.get());
    }

  private:
    QPDFValueProxy(QPDFValueProxy const&) = delete;
    QPDFValueProxy& operator=(QPDFValueProxy const&) = delete;
    std::shared_ptr<QPDFValue> value;
};

#endif // QPDFVALUEPROXY_HH

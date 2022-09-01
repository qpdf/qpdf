#ifndef QPDFOBJECT_HH
#define QPDFOBJECT_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/Types.h>

#include <string>

class QPDF;
class QPDFObjectHandle;

class QPDFObject
{
    friend class QPDFValue;

  public:
    QPDFObject() = default;
    virtual ~QPDFObject() = default;

    std::shared_ptr<QPDFObject>
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
    // Returns nullptr for direct objects
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
    assign(std::shared_ptr<QPDFObject> o)
    {
        value = o->value;
    }
    void
    swapWith(std::shared_ptr<QPDFObject> o)
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
    resetObjGen()
    {
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

    void
    releaseResolved()
    {
        value->releaseResolved();
    }

  private:
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;
    std::shared_ptr<QPDFValue> value;
};

#endif // QPDFOBJECT_HH

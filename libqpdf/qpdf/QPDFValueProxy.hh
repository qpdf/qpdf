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

    void
    setObjGen(QPDF* qpdf, QPDFObjGen const& og)
    {
        // Intended for use by the QPDF class
        value->qpdf = qpdf;
        value->og = og;
    }
    void
    disconnect()
    {
        // Disconnect an object from its owning QPDF. This is called
        // by QPDF's destructor.
        value->disconnect();
        value->qpdf = nullptr;
        value->og = QPDFObjGen();
    }
    // Mark an object as destroyed. Used by QPDF's destructor for its
    // indirect objects.
    void destroy();

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

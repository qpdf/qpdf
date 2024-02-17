#ifndef QPDFOBJECT_HH
#define QPDFOBJECT_HH

// NOTE: This file is called QPDFObject_private.hh instead of QPDFObject.hh because of
// include/qpdf/QPDFObject.hh. See comments there for an explanation.

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/Types.h>

#include <string>
#include <string_view>

class QPDF;
class QPDFObjectHandle;

class QPDFObject
{
    friend class QPDFValue;

  public:
    QPDFObject() = default;

    std::shared_ptr<QPDFObject>
    copy(bool shallow = false)
    {
        return value->copy(shallow);
    }
    std::string
    unparse()
    {
        return value->unparse();
    }
    void
    writeJSON(int json_version, JSON::Writer& p)
    {
        return value->writeJSON(json_version, p);
    }
    std::string
    getStringValue() const
    {
        return value->getStringValue();
    }
    // Return a unique type code for the object
    qpdf_object_type_e
    getTypeCode() const
    {
        return value->type_code;
    }

    // Return a string literal that describes the type, useful for debugging and testing
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
    setDescription(
        QPDF* qpdf, std::shared_ptr<QPDFValue::Description>& description, qpdf_offset_t offset = -1)
    {
        return value->setDescription(qpdf, description, offset);
    }
    void
    setChildDescription(
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr)
    {
        auto qpdf = parent ? parent->value->qpdf : nullptr;
        value->setChildDescription(qpdf, parent->value, static_descr, var_descr);
    }
    void
    setChildDescription(
        std::shared_ptr<QPDFValue> parent,
        std::string_view const& static_descr,
        std::string var_descr)
    {
        auto qpdf = parent ? parent->qpdf : nullptr;
        value->setChildDescription(qpdf, parent, static_descr, var_descr);
    }
    bool
    getDescription(QPDF*& qpdf, std::string& description)
    {
        qpdf = value->qpdf;
        description = value->getDescription();
        return qpdf != nullptr;
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

    void
    setDefaultDescription(QPDF* qpdf, QPDFObjGen const& og)
    {
        // Intended for use by the QPDF class
        value->setDefaultDescription(qpdf, og);
    }
    void
    setObjGen(QPDF* qpdf, QPDFObjGen const& og)
    {
        value->qpdf = qpdf;
        value->og = og;
    }
    void
    disconnect()
    {
        // Disconnect an object from its owning QPDF. This is called by QPDF's destructor.
        value->disconnect();
        value->qpdf = nullptr;
        value->og = QPDFObjGen();
    }
    // Mark an object as destroyed. Used by QPDF's destructor for its indirect objects.
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
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;
    std::shared_ptr<QPDFValue> value;
};

#endif // QPDFOBJECT_HH

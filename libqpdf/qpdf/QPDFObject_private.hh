#ifndef QPDFOBJECT_HH
#define QPDFOBJECT_HH

// NOTE: This file is called QPDFObject_private.hh instead of
// QPDFObject.hh because of include/qpdf/QPDFObject.hh. See comments
// there for an explanation.

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/Types.h>

#include <memory>
#include <optional>
#include <string>

class QPDF;

class QPDFObject: public std::enable_shared_from_this<QPDFObject>
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
    JSON
    getJSON(int json_version)
    {
        return value->getJSON(json_version);
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
    setDescription(
        QPDF* qpdf,
        std::shared_ptr<std::string>& description,
        qpdf_offset_t offset = -1)
    {
        return value->setDescription(qpdf, description, offset);
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
    bool
    isDirectNull() const
    {
        return value->type_code == ::ot_null && !value->og.isIndirect();
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

    // Array methods

    int
    size(bool type_warn = false, bool allow_null = false) const
    {
        return value->size();
    }
    QPDFObjectHandle
    at(int n)
    {
        switch (value->type_code) {
        case ::ot_array:
            return value->at(n);
        case ::ot_null:
        case ::ot_uninitialized:
        case ::ot_reserved:
        case ::ot_unresolved:
        case ::ot_destroyed:
            return {};
        default:
            if (n == 0) {
                return {shared_from_this()};
            } else {
                return {};
            }
        }
    }
    bool
    setAt(int n, QPDFObjectHandle const& oh)
    {
        return value->setAt(n, oh);
    }
    bool
    erase(int at)
    {
        return value->erase(at);
    }
    bool
    insert(int at, QPDFObjectHandle const& item)
    {
        return value->insert(at, item);
    }
    bool
    push_back(QPDFObjectHandle const& item)
    {
        return value->push_back(item);
    }

  private:
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;
    std::shared_ptr<QPDFValue> value;
};

#endif // QPDFOBJECT_HH

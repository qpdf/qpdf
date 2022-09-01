#ifndef QPDFVALUE_HH
#define QPDFVALUE_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/Types.h>

#include <string>

class QPDF;
class QPDFObjectHandle;
class QPDFObject;

class QPDFValue
{
    friend class QPDFObject;

  public:
    virtual ~QPDFValue() = default;

    virtual std::shared_ptr<QPDFObject> shallowCopy() = 0;
    virtual std::string unparse() = 0;
    virtual JSON getJSON(int json_version) = 0;
    virtual void
    setDescription(QPDF* qpdf, std::string const& description)
    {
        owning_qpdf = qpdf;
        object_description = description;
    }
    bool
    getDescription(QPDF*& qpdf, std::string& description)
    {
        qpdf = owning_qpdf;
        description = object_description;
        return owning_qpdf != nullptr;
    }
    bool
    hasDescription()
    {
        return owning_qpdf != nullptr;
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        if (parsed_offset < 0) {
            parsed_offset = offset;
        }
    }
    qpdf_offset_t
    getParsedOffset()
    {
        return parsed_offset;
    }
    QPDF*
    getQPDF()
    {
        return qpdf;
    }
    QPDFObjGen
    getObjGen()
    {
        return og;
    }

  protected:
    QPDFValue() :
        type_code(::ot_uninitialized),
        type_name("uninitialized")
    {
    }
    QPDFValue(qpdf_object_type_e type_code, char const* type_name) :
        type_code(type_code),
        type_name(type_name)
    {
    }
    QPDFValue(
        qpdf_object_type_e type_code,
        char const* type_name,
        QPDF* qpdf,
        QPDFObjGen const& og) :
        type_code(type_code),
        type_name(type_name),
        qpdf(qpdf),
        og(og)
    {
    }
    virtual void
    releaseResolved()
    {
    }
    static std::shared_ptr<QPDFObject> do_create(QPDFValue*);

  private:
    QPDFValue(QPDFValue const&) = delete;
    QPDFValue& operator=(QPDFValue const&) = delete;
    QPDF* owning_qpdf{nullptr};
    std::string object_description;
    qpdf_offset_t parsed_offset{-1};
    const qpdf_object_type_e type_code;
    char const* type_name;

  protected:
    QPDF* qpdf{nullptr};
    QPDFObjGen og;
};

#endif // QPDFVALUE_HH

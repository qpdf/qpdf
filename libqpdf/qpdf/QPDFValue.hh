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
class QPDFValueProxy;

class QPDFValue
{
    friend class QPDFValueProxy;

  public:
    virtual ~QPDFValue() = default;

    virtual std::shared_ptr<QPDFValueProxy> shallowCopy() = 0;
    virtual std::string unparse() = 0;
    virtual JSON getJSON(int json_version) = 0;
    virtual void
    setDescription(QPDF* qpdf_p, std::string const& description)
    {
        qpdf = qpdf_p;
        object_description = description;
    }
    bool
    getDescription(QPDF*& qpdf_p, std::string& description)
    {
        qpdf_p = qpdf;
        description = object_description;
        return qpdf != nullptr;
    }
    bool
    hasDescription()
    {
        return qpdf != nullptr && !object_description.empty();
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
    virtual void
    disconnect()
    {
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

    static std::shared_ptr<QPDFValueProxy> do_create(QPDFValue*);

  private:
    QPDFValue(QPDFValue const&) = delete;
    QPDFValue& operator=(QPDFValue const&) = delete;
    std::string object_description;
    qpdf_offset_t parsed_offset{-1};
    const qpdf_object_type_e type_code;
    char const* type_name;

  protected:
    QPDF* qpdf{nullptr};
    QPDFObjGen og;
};

#endif // QPDFVALUE_HH

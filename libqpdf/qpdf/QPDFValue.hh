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

    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false) = 0;
    virtual std::string unparse() = 0;
    virtual JSON getJSON(int json_version) = 0;
    virtual void
    setDescription(
        QPDF* qpdf_p,
        std::shared_ptr<std::string>& description,
        qpdf_offset_t offset)
    {
        qpdf = qpdf_p;
        object_description = description;
        setParsedOffset(offset);
    }
    void
    setDefaultDescription(QPDF* a_qpdf, QPDFObjGen const& a_og)
    {
        static auto default_description{
            std::make_shared<std::string>("object $OG")};
        if (!object_description) {
            object_description = default_description;
        }
        qpdf = a_qpdf;
        og = a_og;
    }
    std::string
    getDescription()
    {
        auto description = object_description ? *object_description : "";
        if (auto pos = description.find("$OG"); pos != std::string::npos) {
            description.replace(pos, 3, og.unparse(' '));
        }
        if (auto pos = description.find("$PO"); pos != std::string::npos) {
            qpdf_offset_t shift = (type_code == ::ot_dictionary) ? 2
                : (type_code == ::ot_array)                      ? 1
                                                                 : 0;

            description.replace(pos, 3, std::to_string(parsed_offset + shift));
        }
        return description;
    }
    bool
    hasDescription()
    {
        return qpdf != nullptr && object_description &&
            !object_description->empty();
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
    QPDFValue() = default;

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

    static std::shared_ptr<QPDFObject> do_create(QPDFValue*);

  private:
    QPDFValue(QPDFValue const&) = delete;
    QPDFValue& operator=(QPDFValue const&) = delete;
    std::shared_ptr<std::string> object_description;

    const qpdf_object_type_e type_code{::ot_uninitialized};
    char const* type_name{"uninitialized"};

  protected:
    QPDF* qpdf{nullptr};
    QPDFObjGen og{};
    qpdf_offset_t parsed_offset{-1};
};

#endif // QPDFVALUE_HH

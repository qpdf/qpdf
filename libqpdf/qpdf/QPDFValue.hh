#ifndef QPDFVALUE_HH
#define QPDFVALUE_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/Types.h>

#include <string>
#include <string_view>
#include <variant>

class QPDF;
class QPDFObjectHandle;
class QPDFObject;

class QPDFValue: public std::enable_shared_from_this<QPDFValue>
{
    friend class QPDFObject;

  public:
    virtual ~QPDFValue() = default;

    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false) = 0;
    virtual std::string unparse() = 0;
    virtual JSON getJSON(int json_version) = 0;

    struct JSON_Descr
    {
        JSON_Descr(std::shared_ptr<std::string> input, std::string const& object) :
            input(input),
            object(object)
        {
        }

        std::shared_ptr<std::string> input;
        std::string object;
    };

    struct ChildDescr
    {
        ChildDescr(
            std::shared_ptr<QPDFValue> parent,
            std::string_view const& static_descr,
            std::string var_descr) :
            parent(parent),
            static_descr(static_descr),
            var_descr(var_descr)
        {
        }

        std::weak_ptr<QPDFValue> parent;
        std::string_view const& static_descr;
        std::string var_descr;
    };

    using Description = std::variant<std::string, JSON_Descr, ChildDescr>;

    virtual void
    setDescription(QPDF* qpdf_p, std::shared_ptr<Description>& description, qpdf_offset_t offset)
    {
        qpdf = qpdf_p;
        object_description = description;
        setParsedOffset(offset);
    }
    void
    setDefaultDescription(QPDF* a_qpdf, QPDFObjGen const& a_og)
    {
        qpdf = a_qpdf;
        og = a_og;
    }
    void
    setChildDescription(
        QPDF* a_qpdf,
        std::shared_ptr<QPDFValue> parent,
        std::string_view const& static_descr,
        std::string var_descr)
    {
        object_description =
            std::make_shared<Description>(ChildDescr(parent, static_descr, var_descr));
        qpdf = a_qpdf;
    }
    std::string getDescription();
    bool
    hasDescription()
    {
        return object_description || og.isIndirect();
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
    virtual std::string
    getStringValue() const
    {
        return "";
    }

  protected:
    QPDFValue() = default;

    QPDFValue(qpdf_object_type_e type_code, char const* type_name) :
        type_code(type_code),
        type_name(type_name)
    {
    }
    QPDFValue(
        qpdf_object_type_e type_code, char const* type_name, QPDF* qpdf, QPDFObjGen const& og) :
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
    std::shared_ptr<Description> object_description;

    const qpdf_object_type_e type_code{::ot_uninitialized};
    char const* type_name{"uninitialized"};

  protected:
    QPDF* qpdf{nullptr};
    QPDFObjGen og{};
    qpdf_offset_t parsed_offset{-1};
};

#endif // QPDFVALUE_HH

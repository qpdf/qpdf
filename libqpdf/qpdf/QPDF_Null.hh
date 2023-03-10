#ifndef QPDF_NULL_HH
#define QPDF_NULL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Null: public QPDFValue
{
  public:
    virtual ~QPDF_Null() = default;
    static std::shared_ptr<QPDFObject> create();
    static std::shared_ptr<QPDFObject> create(
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr);
    static std::shared_ptr<QPDFObject> create(
        std::shared_ptr<QPDFValue> parent,
        std::string_view const& static_descr,
        std::string var_descr);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

    void
    forEach(
        QPDFObjectHandle& caller,
        std::function<void(int, QPDFObjectHandle&)> fn) final
    {
    }

  private:
    QPDF_Null();
};

#endif // QPDF_NULL_HH

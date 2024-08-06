#ifndef QPDF_NULL_HH
#define QPDF_NULL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Null: public QPDFValue
{
  public:
    ~QPDF_Null() override = default;
    static std::shared_ptr<QPDFObject> create(QPDF* qpdf = nullptr, QPDFObjGen og = QPDFObjGen());
    static std::shared_ptr<QPDFObject> create(
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr);
    static std::shared_ptr<QPDFObject> create(
        std::shared_ptr<QPDFValue> parent,
        std::string_view const& static_descr,
        std::string var_descr);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;

  private:
    QPDF_Null(QPDF* qpdf = nullptr, QPDFObjGen og = QPDFObjGen());
};

#endif // QPDF_NULL_HH

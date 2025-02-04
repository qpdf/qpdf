#ifndef QPDF_UNRESOLVED_HH
#define QPDF_UNRESOLVED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Unresolved: public QPDFValue
{
  public:
    ~QPDF_Unresolved() override = default;
    static std::shared_ptr<QPDFObject> create(QPDF* qpdf, QPDFObjGen og);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    std::string getStringValue() const override;

  private:
    QPDF_Unresolved(QPDF* qpdf, QPDFObjGen og);
};

#endif // QPDF_UNRESOLVED_HH

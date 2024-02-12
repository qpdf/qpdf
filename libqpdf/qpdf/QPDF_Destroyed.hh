#ifndef QPDF_DESTROYED_HH
#define QPDF_DESTROYED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Destroyed: public QPDFValue
{
  public:
    ~QPDF_Destroyed() override = default;
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    static std::shared_ptr<QPDFValue> getInstance();

  private:
    QPDF_Destroyed();
};

#endif // QPDF_DESTROYED_HH

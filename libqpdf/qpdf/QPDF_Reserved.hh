#ifndef QPDF_RESERVED_HH
#define QPDF_RESERVED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Reserved: public QPDFValue
{
  public:
    ~QPDF_Reserved() override = default;
    static std::shared_ptr<QPDFObject> create();
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    JSON getJSON(int json_version) override;

  private:
    QPDF_Reserved();
};

#endif // QPDF_RESERVED_HH

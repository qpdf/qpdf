#ifndef QPDF_INTEGER_HH
#define QPDF_INTEGER_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Integer: public QPDFValue
{
  public:
    ~QPDF_Integer() override = default;
    static std::shared_ptr<QPDFObject> create(long long value);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    JSON getJSON(int json_version) override;
    long long getVal() const;

  private:
    QPDF_Integer(long long val);
    long long val;
};

#endif // QPDF_INTEGER_HH

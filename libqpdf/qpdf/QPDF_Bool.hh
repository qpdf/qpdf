#ifndef QPDF_BOOL_HH
#define QPDF_BOOL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Bool: public QPDFValue
{
  public:
    ~QPDF_Bool() override = default;
    static std::shared_ptr<QPDFObject> create(bool val);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;

    bool getVal() const;

  private:
    QPDF_Bool(bool val);
    bool val;
};

#endif // QPDF_BOOL_HH

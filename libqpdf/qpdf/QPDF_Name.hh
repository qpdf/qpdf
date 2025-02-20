#ifndef QPDF_NAME_HH
#define QPDF_NAME_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Name: public QPDFValue
{
  public:
    ~QPDF_Name() override = default;
    static std::shared_ptr<QPDFObject> create(std::string const& name);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    std::string
    getStringValue() const override
    {
        return name;
    }

  private:
    QPDF_Name(std::string const& name);
    std::string name;
};

#endif // QPDF_NAME_HH

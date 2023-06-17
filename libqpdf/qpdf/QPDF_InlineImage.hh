#ifndef QPDF_INLINEIMAGE_HH
#define QPDF_INLINEIMAGE_HH

#include <qpdf/QPDFValue.hh>

class QPDF_InlineImage: public QPDFValue
{
  public:
    ~QPDF_InlineImage() override = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    JSON getJSON(int json_version) override;
    std::string
    getStringValue() const override
    {
        return val;
    }

  private:
    QPDF_InlineImage(std::string const& val);
    std::string val;
};

#endif // QPDF_INLINEIMAGE_HH

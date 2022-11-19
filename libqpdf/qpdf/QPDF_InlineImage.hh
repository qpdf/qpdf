#ifndef QPDF_INLINEIMAGE_HH
#define QPDF_INLINEIMAGE_HH

#include <qpdf/QPDFValue.hh>

class QPDF_InlineImage: public QPDFValue
{
  public:
    virtual ~QPDF_InlineImage() = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    std::string getVal() const;

    static constexpr const char* NAME = "inline-image";
    static constexpr qpdf_object_type_e CODE = ot_inlineimage;

  private:
    QPDF_InlineImage(std::string const& val);
    std::string val;
};

#endif // QPDF_INLINEIMAGE_HH

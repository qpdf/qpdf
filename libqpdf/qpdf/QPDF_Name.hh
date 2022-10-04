#ifndef QPDF_NAME_HH
#define QPDF_NAME_HH

#include <qpdf/QPDFValue.hh>

#include <string_view>

class QPDF_Name: public QPDFValue
{
  public:
    virtual ~QPDF_Name() = default;
    static std::shared_ptr<QPDFObject> create(std::string_view name);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    std::string getName() const;

    // Put # into strings with characters unsuitable for name token
    static std::string normalizeName(std::string_view name);

  private:
    QPDF_Name(std::string_view name);
    std::string name;
};

#endif // QPDF_NAME_HH

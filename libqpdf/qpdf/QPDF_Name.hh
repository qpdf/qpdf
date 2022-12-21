#ifndef QPDF_NAME_HH
#define QPDF_NAME_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Name: public QPDFValue
{
  public:
    virtual ~QPDF_Name() = default;
    static std::shared_ptr<QPDFObject> create(std::string const& name);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

    // Put # into strings with characters unsuitable for name token
    static std::string normalizeName(std::string const& name);
    virtual std::string
    getStringValue() const
    {
        return name;
    }

  private:
    QPDF_Name(std::string const& name);
    std::string name;
};

#endif // QPDF_NAME_HH

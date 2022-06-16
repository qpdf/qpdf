#ifndef QPDF_NAME_HH
#define QPDF_NAME_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Name: public QPDFObject
{
  public:
    virtual ~QPDF_Name() = default;
    static std::shared_ptr<QPDFObject> create(std::string const& name);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string getName() const;

    // Put # into strings with characters unsuitable for name token
    static std::string normalizeName(std::string const& name);

  private:
    QPDF_Name(std::string const& name);
    std::string name;
};

#endif // QPDF_NAME_HH

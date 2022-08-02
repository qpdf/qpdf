#ifndef QPDF_UNRESOLVED_HH
#define QPDF_UNRESOLVED_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Unresolved: public QPDFObject
{
  public:
    virtual ~QPDF_Unresolved() = default;
    static std::shared_ptr<QPDFObject> create();
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;

  private:
    QPDF_Unresolved() = default;
};

#endif // QPDF_UNRESOLVED_HH

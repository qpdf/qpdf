#ifndef QPDF_NULL_HH
#define QPDF_NULL_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Null: public QPDFObject
{
  public:
    virtual ~QPDF_Null() = default;
    static std::shared_ptr<QPDFObject> create();
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
  private:
    QPDF_Null() = default;
};

#endif // QPDF_NULL_HH

#ifndef QPDF_NULL_HH
#define QPDF_NULL_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Null: public QPDFObject
{
  public:
    virtual ~QPDF_Null();
    virtual std::string unparse();
    virtual JSON getJSON();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
};

#endif // QPDF_NULL_HH

#ifndef __QPDF_NULL_HH__
#define __QPDF_NULL_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Null: public QPDFObject
{
  public:
    virtual ~QPDF_Null();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
};

#endif // __QPDF_NULL_HH__

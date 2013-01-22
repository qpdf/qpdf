#ifndef __QPDF_RESERVED_HH__
#define __QPDF_RESERVED_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Reserved: public QPDFObject
{
  public:
    virtual ~QPDF_Reserved();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
};

#endif // __QPDF_RESERVED_HH__

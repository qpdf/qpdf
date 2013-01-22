#ifndef __QPDF_INTEGER_HH__
#define __QPDF_INTEGER_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Integer: public QPDFObject
{
  public:
    QPDF_Integer(long long val);
    virtual ~QPDF_Integer();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    long long getVal() const;

  private:
    long long val;
};

#endif // __QPDF_INTEGER_HH__

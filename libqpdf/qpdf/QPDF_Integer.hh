#ifndef QPDF_INTEGER_HH
#define QPDF_INTEGER_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Integer: public QPDFObject
{
  public:
    QPDF_Integer(long long val);
    virtual ~QPDF_Integer() = default;
    virtual std::string unparse();
    virtual JSON getJSON();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    long long getVal() const;

  private:
    long long val;
};

#endif // QPDF_INTEGER_HH

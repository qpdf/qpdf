#ifndef QPDF_OPERATOR_HH
#define QPDF_OPERATOR_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Operator: public QPDFObject
{
  public:
    QPDF_Operator(std::string const& val);
    virtual ~QPDF_Operator();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string getVal() const;

  private:
    std::string val;
};

#endif // QPDF_OPERATOR_HH

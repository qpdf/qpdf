
#ifndef __QPDF_INTEGER_HH__
#define __QPDF_INTEGER_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Integer: public QPDFObject
{
  public:
    QPDF_Integer(int val);
    virtual ~QPDF_Integer();
    virtual std::string unparse();
    int getVal() const;

  private:
    int val;
};

#endif // __QPDF_INTEGER_HH__

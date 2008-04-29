
#ifndef __QPDF_REAL_HH__
#define __QPDF_REAL_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Real: public QPDFObject
{
  public:
    QPDF_Real(std::string const& val);
    virtual ~QPDF_Real();
    std::string unparse();
    std::string getVal();

  private:
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

#endif // __QPDF_REAL_HH__

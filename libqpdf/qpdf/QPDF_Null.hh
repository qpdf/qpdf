#ifndef __QPDF_NULL_HH__
#define __QPDF_NULL_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Null: public QPDFObject
{
  public:
    virtual ~QPDF_Null();
    std::string unparse();
};

#endif // __QPDF_NULL_HH__

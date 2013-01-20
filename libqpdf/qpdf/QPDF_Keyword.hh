#ifndef __QPDF_KEYWORD_HH__
#define __QPDF_KEYWORD_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Keyword: public QPDFObject
{
  public:
    QPDF_Keyword(std::string const& val);
    virtual ~QPDF_Keyword();
    virtual std::string unparse();
    std::string getVal() const;

  private:
    std::string val;
};

#endif // __QPDF_KEYWORD_HH__

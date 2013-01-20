#ifndef __QPDF_INLINEIMAGE_HH__
#define __QPDF_INLINEIMAGE_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_InlineImage: public QPDFObject
{
  public:
    QPDF_InlineImage(std::string const& val);
    virtual ~QPDF_InlineImage();
    virtual std::string unparse();
    std::string getVal() const;

  private:
    std::string val;
};

#endif // __QPDF_INLINEIMAGE_HH__

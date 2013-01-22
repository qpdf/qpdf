#ifndef __QPDF_INLINEIMAGE_HH__
#define __QPDF_INLINEIMAGE_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_InlineImage: public QPDFObject
{
  public:
    QPDF_InlineImage(std::string const& val);
    virtual ~QPDF_InlineImage();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string getVal() const;

  private:
    std::string val;
};

#endif // __QPDF_INLINEIMAGE_HH__

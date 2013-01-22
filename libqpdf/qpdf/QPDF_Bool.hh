#ifndef __QPDF_BOOL_HH__
#define __QPDF_BOOL_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Bool: public QPDFObject
{
  public:
    QPDF_Bool(bool val);
    virtual ~QPDF_Bool();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    bool getVal() const;

  private:
    bool val;
};

#endif // __QPDF_BOOL_HH__

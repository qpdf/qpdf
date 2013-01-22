#ifndef __QPDF_STRING_HH__
#define __QPDF_STRING_HH__

#include <qpdf/QPDFObject.hh>

// QPDF_Strings may included embedded null characters.

class QPDF_String: public QPDFObject
{
  public:
    QPDF_String(std::string const& val);
    virtual ~QPDF_String();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string unparse(bool force_binary);
    std::string getVal() const;
    std::string getUTF8Val() const;

  private:
    std::string val;
};

#endif // __QPDF_STRING_HH__

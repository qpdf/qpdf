#ifndef QPDF_STRING_HH
#define QPDF_STRING_HH

#include <qpdf/QPDFObject.hh>

// QPDF_Strings may included embedded null characters.

class QPDF_String: public QPDFObject
{
  public:
    QPDF_String(std::string const& val);
    static QPDF_String* new_utf16(std::string const& utf8_val);
    virtual ~QPDF_String() = default;
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string unparse(bool force_binary);
    virtual JSON getJSON();
    std::string getVal() const;
    std::string getUTF8Val() const;

  private:
    std::string val;
};

#endif // QPDF_STRING_HH

#ifndef QPDF_STRING_HH
#define QPDF_STRING_HH

#include <qpdf/QPDFValue.hh>

// QPDF_Strings may included embedded null characters.

class QPDF_String: public QPDFValue
{
    friend class QPDFWriter;

  public:
    virtual ~QPDF_String() = default;
    static std::shared_ptr<QPDFObject> create(std::string_view val);
    static std::shared_ptr<QPDFObject> create_utf16(std::string_view utf8_val);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    std::string unparse(bool force_binary);
    virtual JSON getJSON(int json_version);
    std::string getVal() const;
    std::string getUTF8Val() const;

  private:
    QPDF_String(std::string_view val);
    bool useHexString() const;
    std::string val;
};

#endif // QPDF_STRING_HH

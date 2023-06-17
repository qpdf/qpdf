#ifndef QPDF_STRING_HH
#define QPDF_STRING_HH

#include <qpdf/QPDFValue.hh>

// QPDF_Strings may included embedded null characters.

class QPDF_String: public QPDFValue
{
    friend class QPDFWriter;

  public:
    ~QPDF_String() override = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    static std::shared_ptr<QPDFObject> create_utf16(std::string const& utf8_val);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    std::string unparse(bool force_binary);
    JSON getJSON(int json_version) override;
    std::string getUTF8Val() const;
    std::string
    getStringValue() const override
    {
        return val;
    }

  private:
    QPDF_String(std::string const& val);
    bool useHexString() const;
    std::string val;
};

#endif // QPDF_STRING_HH

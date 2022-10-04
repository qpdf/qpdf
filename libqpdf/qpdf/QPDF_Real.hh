#ifndef QPDF_REAL_HH
#define QPDF_REAL_HH

#include <qpdf/QPDFValue.hh>

#include <string_view>

class QPDF_Real: public QPDFValue
{
  public:
    virtual ~QPDF_Real() = default;
    static std::shared_ptr<QPDFObject> create(std::string_view val);
    static std::shared_ptr<QPDFObject>
    create(double value, int decimal_places, bool trim_trailing_zeroes);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual std::string
    getStringValue() const
    {
        return val;
    }

  private:
    QPDF_Real(std::string_view val);
    QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes);
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

#endif // QPDF_REAL_HH

#ifndef QPDF_REAL_HH
#define QPDF_REAL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Real: public QPDFValue
{
  public:
    virtual ~QPDF_Real() = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    static std::shared_ptr<QPDFObject>
    create(double value, int decimal_places, bool trim_trailing_zeroes);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    std::string getVal();

    static constexpr const char* NAME = "real";

  private:
    QPDF_Real(std::string const& val);
    QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes);
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

#endif // QPDF_REAL_HH

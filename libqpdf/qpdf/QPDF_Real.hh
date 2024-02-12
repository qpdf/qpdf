#ifndef QPDF_REAL_HH
#define QPDF_REAL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Real: public QPDFValue
{
  public:
    ~QPDF_Real() override = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    static std::shared_ptr<QPDFObject>
    create(double value, int decimal_places, bool trim_trailing_zeroes);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    std::string
    getStringValue() const override
    {
        return val;
    }

  private:
    QPDF_Real(std::string const& val);
    QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes);
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

#endif // QPDF_REAL_HH

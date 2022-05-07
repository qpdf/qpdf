#ifndef QPDF_REAL_HH
#define QPDF_REAL_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Real: public QPDFObject
{
  public:
    QPDF_Real(std::string const& val);
    QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes);
    virtual ~QPDF_Real() = default;
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string getVal();

  private:
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

#endif // QPDF_REAL_HH

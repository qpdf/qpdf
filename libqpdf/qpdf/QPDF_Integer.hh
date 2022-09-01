#ifndef QPDF_INTEGER_HH
#define QPDF_INTEGER_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Integer: public QPDFValue
{
  public:
    virtual ~QPDF_Integer() = default;
    static std::shared_ptr<QPDFObject> create(long long value);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    long long getVal() const;

  private:
    QPDF_Integer(long long val);
    long long val;
};

#endif // QPDF_INTEGER_HH

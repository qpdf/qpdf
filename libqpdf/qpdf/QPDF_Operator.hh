#ifndef QPDF_OPERATOR_HH
#define QPDF_OPERATOR_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Operator: public QPDFValue
{
  public:
    virtual ~QPDF_Operator() = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual std::string
    getStringValue() const
    {
        return val;
    }

  private:
    QPDF_Operator(std::string const& val);
    std::string val;
};

#endif // QPDF_OPERATOR_HH

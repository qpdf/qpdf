#ifndef QPDF_OPERATOR_HH
#define QPDF_OPERATOR_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Operator: public QPDFValue
{
  public:
    virtual ~QPDF_Operator() = default;
    static std::shared_ptr<QPDFObject> create(std::string_view val);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    std::string getVal() const;

  private:
    QPDF_Operator(std::string_view val);
    std::string val;
};

#endif // QPDF_OPERATOR_HH

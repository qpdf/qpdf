#ifndef QPDF_OPERATOR_HH
#define QPDF_OPERATOR_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Operator: public QPDFObject
{
  public:
    virtual ~QPDF_Operator() = default;
    static std::shared_ptr<QPDFObject> create(std::string const& val);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    std::string getVal() const;

  private:
    QPDF_Operator(std::string const& val);
    std::string val;
};

#endif // QPDF_OPERATOR_HH

#ifndef QPDF_NULL_HH
#define QPDF_NULL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Null: public QPDFValue
{
  public:
    virtual ~QPDF_Null() = default;
    static std::shared_ptr<QPDFObject> create();
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

    static constexpr const char* NAME = "null";

  private:
    QPDF_Null();
};

#endif // QPDF_NULL_HH

#ifndef QPDF_BOOL_HH
#define QPDF_BOOL_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Bool: public QPDFValue
{
  public:
    virtual ~QPDF_Bool() = default;
    static std::shared_ptr<QPDFValueProxy> create(bool val);
    virtual std::shared_ptr<QPDFValueProxy> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    bool getVal() const;

  private:
    QPDF_Bool(bool val);
    bool val;
};

#endif // QPDF_BOOL_HH

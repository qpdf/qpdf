#ifndef QPDF_RESERVED_HH
#define QPDF_RESERVED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Reserved: public QPDFValue
{
  public:
    virtual ~QPDF_Reserved() = default;
    static std::shared_ptr<QPDFValueProxy> create();
    virtual std::shared_ptr<QPDFValueProxy> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

  private:
    QPDF_Reserved();
};

#endif // QPDF_RESERVED_HH

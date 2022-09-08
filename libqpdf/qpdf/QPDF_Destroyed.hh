#ifndef QPDF_DESTROYED_HH
#define QPDF_DESTROYED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Destroyed: public QPDFValue
{
  public:
    virtual ~QPDF_Destroyed() = default;
    virtual std::shared_ptr<QPDFValueProxy> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    static std::shared_ptr<QPDFValue> getInstance();

  private:
    QPDF_Destroyed();
};

#endif // QPDF_DESTROYED_HH

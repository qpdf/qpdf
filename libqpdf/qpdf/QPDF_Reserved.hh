#ifndef QPDF_RESERVED_HH
#define QPDF_RESERVED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Reserved: public QPDFValue
{
  public:
    virtual ~QPDF_Reserved() = default;
    static std::shared_ptr<QPDFObject> create();
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

    static constexpr const char* NAME = "reserved";

  private:
    QPDF_Reserved();
};

#endif // QPDF_RESERVED_HH

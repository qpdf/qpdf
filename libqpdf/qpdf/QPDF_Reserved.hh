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
    virtual qpdf_object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;

  private:
    QPDF_Reserved() = default;
};

#endif // QPDF_RESERVED_HH

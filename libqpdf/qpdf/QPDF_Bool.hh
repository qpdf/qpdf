#ifndef QPDF_BOOL_HH
#define QPDF_BOOL_HH

#include <qpdf/QPDFObject.hh>

class QPDF_Bool: public QPDFObject
{
  public:
    virtual ~QPDF_Bool() = default;
    static std::shared_ptr<QPDFObject> create(bool val);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    bool getVal() const;

  private:
    QPDF_Bool(bool val);
    bool val;
};

#endif // QPDF_BOOL_HH

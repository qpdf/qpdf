#ifndef QPDF_DESTROYED_HH
#define QPDF_DESTROYED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Destroyed: public QPDFValue
{
  public:
    virtual ~QPDF_Destroyed() = default;
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    static std::shared_ptr<QPDFValue> getInstance();

    static constexpr const char* NAME = "destroyed";
    static constexpr qpdf_object_type_e CODE = ot_destroyed;

  private:
    QPDF_Destroyed();
};

#endif // QPDF_DESTROYED_HH

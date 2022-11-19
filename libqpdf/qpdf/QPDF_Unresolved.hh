#ifndef QPDF_UNRESOLVED_HH
#define QPDF_UNRESOLVED_HH

#include <qpdf/QPDFValue.hh>

class QPDF_Unresolved: public QPDFValue
{
  public:
    virtual ~QPDF_Unresolved() = default;
    static std::shared_ptr<QPDFObject> create(QPDF* qpdf, QPDFObjGen const& og);
    virtual std::shared_ptr<QPDFObject> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

    static constexpr const char* NAME = "unresolved";

  private:
    QPDF_Unresolved(QPDF* qpdf, QPDFObjGen const& og);
};

#endif // QPDF_UNRESOLVED_HH

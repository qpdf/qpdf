
#ifndef __QPDF_NAME_HH__
#define __QPDF_NAME_HH__

#include <qpdf/QPDFObject.hh>

class QPDF_Name: public QPDFObject
{
  public:
    QPDF_Name(std::string const& name);
    virtual ~QPDF_Name();
    virtual std::string unparse();
    std::string getName() const;

    // Put # into strings with characters unsuitable for name token
    static std::string normalizeName(std::string const& name);

  private:
    std::string name;
};

#endif // __QPDF_NAME_HH__

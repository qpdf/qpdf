
#ifndef __QPDF_ARRAY_HH__
#define __QPDF_ARRAY_HH__

#include <qpdf/QPDFObject.hh>

#include <vector>
#include <qpdf/QPDFObjectHandle.hh>

class QPDF_Array: public QPDFObject
{
  public:
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    virtual ~QPDF_Array();
    virtual std::string unparse();
    int getNItems() const;
    QPDFObjectHandle getItem(int n) const;
    void setItem(int, QPDFObjectHandle const&);

  private:
    std::vector<QPDFObjectHandle> items;
};

#endif // __QPDF_ARRAY_HH__

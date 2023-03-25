#ifndef QPDF_OHARRAY_HH
#define QPDF_OHARRAY_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>

#include <vector>

class QPDF_Array;

class OHArray
{
  public:
    OHArray();
    QPDFObjectHandle at(size_t idx) const;
    void setAt(size_t idx, QPDFObjectHandle oh);
    void erase(size_t idx);
    void insert(size_t idx, QPDFObjectHandle oh);
    OHArray copy();
    void disconnect();

  private:
    friend class QPDF_Array;
    std::vector<std::shared_ptr<QPDFObject>> elements;
};

#endif // QPDF_OHARRAY_HH

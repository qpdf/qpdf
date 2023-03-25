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

  private:
    friend class QPDF_Array;
    std::vector<std::shared_ptr<QPDFObject>> elements;
};

#endif // QPDF_OHARRAY_HH

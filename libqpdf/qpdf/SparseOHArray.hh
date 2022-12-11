#ifndef QPDF_SPARSEOHARRAY_HH
#define QPDF_SPARSEOHARRAY_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <map>

class QPDF_Array;

class SparseOHArray
{
  public:
    SparseOHArray() = default;
    int
    size() const noexcept
    {
        return n_elements;
    }
    SparseOHArray copy();

  private:
    friend class QPDF_Array;
    std::map<int, std::shared_ptr<QPDFObject>> elements;
    int n_elements{0};
};

#endif // QPDF_SPARSEOHARRAY_HH

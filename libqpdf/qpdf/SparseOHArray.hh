#ifndef QPDF_SPARSEOHARRAY_HH
#define QPDF_SPARSEOHARRAY_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <unordered_map>

class SparseOHArray
{
  public:
    SparseOHArray();
    size_t size() const;
    void append(QPDFObjectHandle oh);
    QPDFObjectHandle at(size_t idx) const;
    void remove_last();
    void releaseResolved();
    void setAt(size_t idx, QPDFObjectHandle oh);
    void erase(size_t idx);
    void insert(size_t idx, QPDFObjectHandle oh);

    typedef std::unordered_map<size_t, QPDFObjectHandle>::const_iterator
        const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

  private:
    std::unordered_map<size_t, QPDFObjectHandle> elements;
    size_t n_elements;
};

#endif // QPDF_SPARSEOHARRAY_HH

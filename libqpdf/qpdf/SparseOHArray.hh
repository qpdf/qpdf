#ifndef QPDF_SPARSEOHARRAY_HH
#define QPDF_SPARSEOHARRAY_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <map>

class SparseOHArray
{
  public:
    QPDF_DLL
    SparseOHArray();
    QPDF_DLL
    size_t size() const;
    QPDF_DLL
    void append(QPDFObjectHandle oh);
    QPDF_DLL
    QPDFObjectHandle at(size_t idx) const;
    QPDF_DLL
    void remove_last();
    QPDF_DLL
    void releaseResolved();
    QPDF_DLL
    void setAt(size_t idx, QPDFObjectHandle oh);
    QPDF_DLL
    void erase(size_t idx);
    QPDF_DLL
    void insert(size_t idx, QPDFObjectHandle oh);

    typedef std::map<size_t, QPDFObjectHandle>::const_iterator const_iterator;
    QPDF_DLL
    const_iterator begin() const;
    QPDF_DLL
    const_iterator end() const;

  private:
    std::map<size_t, QPDFObjectHandle> elements;
    size_t n_elements;
};

#endif // QPDF_SPARSEOHARRAY_HH

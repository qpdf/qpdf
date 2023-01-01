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
    void
    append(QPDFObjectHandle oh)
    {
        elements[n_elements++] = oh.getObj();
    }
    void
    append(std::shared_ptr<QPDFObject>&& obj)
    {
        elements[n_elements++] = std::move(obj);
    }
    QPDFObjectHandle at(int idx) const;
    void remove_last();
    void
    setAt(int idx, QPDFObjectHandle oh)
    {
        elements[idx] = oh.getObj();
    }
    void erase(int idx);
    void insert(int idx, QPDFObjectHandle oh);
    SparseOHArray copy();
    void disconnect();

    typedef std::map<int, std::shared_ptr<QPDFObject>>::const_iterator
        const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

  private:
    friend class QPDF_Array;
    std::map<int, std::shared_ptr<QPDFObject>> elements;
    int n_elements{0};
};

#endif // QPDF_SPARSEOHARRAY_HH

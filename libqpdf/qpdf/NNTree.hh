#ifndef NNTREE_HH
#define NNTREE_HH

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include <iterator>
#include <list>

class NNTreeDetails
{
  public:
    virtual std::string const& itemsKey() const = 0;
    virtual bool keyValid(QPDFObjectHandle) const = 0;
    virtual int compareKeys(QPDFObjectHandle, QPDFObjectHandle) const = 0;
};

class NNTreeIterator: public std::iterator<
    std::bidirectional_iterator_tag,
    std::pair<QPDFObjectHandle, QPDFObjectHandle>,
    void,
    std::pair<QPDFObjectHandle, QPDFObjectHandle>*,
    std::pair<QPDFObjectHandle, QPDFObjectHandle>>
{
    friend class NNTreeImpl;
  public:
    bool valid() const;
    NNTreeIterator& operator++();
    NNTreeIterator operator++(int)
    {
        NNTreeIterator t = *this;
        ++(*this);
        return t;
    }
    NNTreeIterator& operator--();
    NNTreeIterator operator--(int)
    {
        NNTreeIterator t = *this;
        --(*this);
        return t;
    }
    reference operator*();
    bool operator==(NNTreeIterator const& other) const;
    bool operator!=(NNTreeIterator const& other) const
    {
        return ! operator==(other);
    }

  private:
    class PathElement
    {
      public:
        PathElement(QPDFObjectHandle const& node, int kid_number);
        QPDFObjectHandle getNextKid(bool backward);

        QPDFObjectHandle node;
        int kid_number;
    };

    // ABI: for qpdf 11, make qpdf a reference
    NNTreeIterator(NNTreeDetails const& details, QPDF* qpdf) :
        details(details),
        qpdf(qpdf),
        item_number(-1)
    {
    }
    void reset();
    void deepen(QPDFObjectHandle node, bool first);
    void setItemNumber(QPDFObjectHandle const& node, int);
    void addPathElement(QPDFObjectHandle const& node, int kid_number);
    void increment(bool backward);

    NNTreeDetails const& details;
    QPDF* qpdf;
    std::list<PathElement> path;
    QPDFObjectHandle node;
    int item_number;
};

class NNTreeImpl
{
  public:
    typedef NNTreeIterator iterator;

    NNTreeImpl(NNTreeDetails const&, QPDF*, QPDFObjectHandle&,
               bool auto_repair = true);
    iterator begin();
    iterator end();
    iterator last();
    iterator find(QPDFObjectHandle key, bool return_prev_if_not_found = false);

  private:
    int withinLimits(QPDFObjectHandle key, QPDFObjectHandle node);
    int binarySearch(
        QPDFObjectHandle key, QPDFObjectHandle items,
        int num_items, bool return_prev_if_not_found,
        int (NNTreeImpl::*compare)(QPDFObjectHandle& key,
                                 QPDFObjectHandle& node,
                                 int item));
    int compareKeyItem(
        QPDFObjectHandle& key, QPDFObjectHandle& items, int idx);
    int compareKeyKid(
        QPDFObjectHandle& key, QPDFObjectHandle& items, int idx);

    NNTreeDetails const& details;
    QPDF* qpdf;
    QPDFObjectHandle oh;
};

#endif // NNTREE_HH

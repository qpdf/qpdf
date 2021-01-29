#ifndef NNTREE_HH
#define NNTREE_HH

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include <iterator>
#include <list>
#include <memory>

class NNTreeDetails
{
  public:
    virtual std::string const& itemsKey() const = 0;
    virtual bool keyValid(QPDFObjectHandle) const = 0;
    virtual int compareKeys(QPDFObjectHandle, QPDFObjectHandle) const = 0;
};

class NNTreeImpl;
class NNTreeIterator: public std::iterator<
    std::bidirectional_iterator_tag,
    std::pair<QPDFObjectHandle, QPDFObjectHandle>>
{
    friend class NNTreeImpl;
  public:
    virtual ~NNTreeIterator() = default;
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
    pointer operator->();
    bool operator==(NNTreeIterator const& other) const;
    bool operator!=(NNTreeIterator const& other) const
    {
        return ! operator==(other);
    }

    void insertAfter(
        QPDFObjectHandle key, QPDFObjectHandle value);
    void remove();

  private:
    class PathElement
    {
      public:
        PathElement(QPDFObjectHandle const& node, int kid_number);

        QPDFObjectHandle node;
        int kid_number;
    };

    // ABI: for qpdf 11, make qpdf a reference
    NNTreeIterator(NNTreeImpl& impl);
    void updateIValue(bool allow_invalid = true);
    bool deepen(QPDFObjectHandle node, bool first, bool allow_empty);
    void setItemNumber(QPDFObjectHandle const& node, int);
    void addPathElement(QPDFObjectHandle const& node, int kid_number);
    QPDFObjectHandle getNextKid(PathElement& element, bool backward);
    void increment(bool backward);
    void resetLimits(QPDFObjectHandle node,
                     std::list<PathElement>::iterator parent);

    void split(QPDFObjectHandle to_split,
               std::list<PathElement>::iterator parent);
    std::list<PathElement>::iterator lastPathElement();

    NNTreeImpl& impl;
    std::list<PathElement> path;
    QPDFObjectHandle node;
    int item_number;
    value_type ivalue;
};

class NNTreeImpl
{
    friend class NNTreeIterator;
  public:
    typedef NNTreeIterator iterator;

    NNTreeImpl(NNTreeDetails const&, QPDF*, QPDFObjectHandle&,
               bool auto_repair = true);
    iterator begin();
    iterator end();
    iterator last();
    iterator find(QPDFObjectHandle key, bool return_prev_if_not_found = false);
    iterator insertFirst(QPDFObjectHandle key, QPDFObjectHandle value);
    iterator insert(QPDFObjectHandle key, QPDFObjectHandle value);
    bool remove(QPDFObjectHandle key, QPDFObjectHandle* value = nullptr);

    // Change the split threshold for easier testing. There's no real
    // reason to expose this to downstream tree helpers, but it has to
    // be public so we can call it from the test suite.
    void setSplitThreshold(int split_threshold);

  private:
    void repair();
    iterator findInternal(
        QPDFObjectHandle key, bool return_prev_if_not_found = false);
    int withinLimits(QPDFObjectHandle key, QPDFObjectHandle node);
    int binarySearch(
        QPDFObjectHandle key, QPDFObjectHandle items,
        int num_items, bool return_prev_if_not_found,
        int (NNTreeImpl::*compare)(QPDFObjectHandle& key,
                                 QPDFObjectHandle& arr,
                                 int item));
    int compareKeyItem(
        QPDFObjectHandle& key, QPDFObjectHandle& items, int idx);
    int compareKeyKid(
        QPDFObjectHandle& key, QPDFObjectHandle& items, int idx);

    NNTreeDetails const& details;
    QPDF* qpdf;
    int split_threshold;
    QPDFObjectHandle oh;
    bool auto_repair;
};

#endif // NNTREE_HH

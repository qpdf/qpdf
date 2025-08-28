#ifndef NNTREE_HH
#define NNTREE_HH

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle_private.hh>

#include <iterator>
#include <list>
#include <memory>

class NNTreeImpl;
class NNTreeIterator
{
    friend class NNTreeImpl;

  public:
    typedef std::pair<QPDFObjectHandle, QPDFObjectHandle> T;
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = long;
    using pointer = T*;
    using reference = T&;

    virtual ~NNTreeIterator() = default;
    bool valid() const;
    NNTreeIterator& operator++();
    NNTreeIterator
    operator++(int)
    {
        NNTreeIterator t = *this;
        ++(*this);
        return t;
    }
    NNTreeIterator& operator--();
    NNTreeIterator
    operator--(int)
    {
        NNTreeIterator t = *this;
        --(*this);
        return t;
    }
    reference operator*();
    pointer operator->();
    bool operator==(NNTreeIterator const& other) const;
    bool
    operator!=(NNTreeIterator const& other) const
    {
        return !operator==(other);
    }

    void insertAfter(QPDFObjectHandle const& key, QPDFObjectHandle const& value);
    void remove();

  private:
    class PathElement
    {
      public:
        PathElement(QPDFObjectHandle const& node, int kid_number);

        QPDFObjectHandle node;
        int kid_number;
    };

    NNTreeIterator(NNTreeImpl& impl);
    void updateIValue(bool allow_invalid = true);
    bool deepen(QPDFObjectHandle node, bool first, bool allow_empty);
    void setItemNumber(QPDFObjectHandle const& node, int);
    void addPathElement(QPDFObjectHandle const& node, int kid_number);
    QPDFObjectHandle getNextKid(PathElement& element, bool backward);
    void increment(bool backward);
    void resetLimits(QPDFObjectHandle node, std::list<PathElement>::iterator parent);

    void split(QPDFObjectHandle to_split, std::list<PathElement>::iterator parent);
    std::list<PathElement>::iterator lastPathElement();

    NNTreeImpl& impl;
    std::list<PathElement> path;
    QPDFObjectHandle node;
    int item_number{-1};
    value_type ivalue;
};

class NNTreeImpl
{
    friend class NNTreeIterator;

  public:
    typedef NNTreeIterator iterator;

    NNTreeImpl(
        QPDF&,
        QPDFObjectHandle&,
        qpdf_object_type_e key_type,
        std::function<bool(QPDFObjectHandle const&)> value_validator,
        bool auto_repair = true);
    iterator begin();
    iterator end();
    iterator last();
    iterator find(QPDFObjectHandle key, bool return_prev_if_not_found = false);
    iterator insertFirst(QPDFObjectHandle const& key, QPDFObjectHandle const& value);
    iterator insert(QPDFObjectHandle const& key, QPDFObjectHandle const& value);
    bool remove(QPDFObjectHandle const& key, QPDFObjectHandle* value = nullptr);

    bool validate(bool repair = true);

    // Change the split threshold for easier testing. There's no real reason to expose this to
    // downstream tree helpers, but it has to be public so we can call it from the test suite.
    void setSplitThreshold(int split_threshold);

  private:
    void repair();
    iterator findInternal(QPDFObjectHandle const& key, bool return_prev_if_not_found = false);
    int withinLimits(QPDFObjectHandle const& key, QPDFObjectHandle const& node);
    int binarySearch(
        QPDFObjectHandle key,
        QPDFObjectHandle items,
        size_t num_items,
        bool return_prev_if_not_found,
        int (NNTreeImpl::*compare)(QPDFObjectHandle& key, QPDFObjectHandle& arr, int item));
    int compareKeyItem(QPDFObjectHandle& key, QPDFObjectHandle& items, int idx);
    int compareKeyKid(QPDFObjectHandle& key, QPDFObjectHandle& items, int idx);
    void warn(QPDFObjectHandle const& node, std::string const& msg);
    void error(QPDFObjectHandle const& node, std::string const& msg);

    std::string const&
    itemsKey() const
    {
        return items_key;
    }
    bool
    keyValid(QPDFObjectHandle o) const
    {
        return o.resolved_type_code() == key_type;
    }
    int
    compareKeys(QPDFObjectHandle a, QPDFObjectHandle b) const;

    QPDF& qpdf;
    int split_threshold{32};
    QPDFObjectHandle oh;
    const qpdf_object_type_e key_type;
    const std::string items_key;
    const std::function<bool(QPDFObjectHandle const&)> value_valid;
    bool auto_repair{true};
    size_t error_count{0};
};

#endif // NNTREE_HH

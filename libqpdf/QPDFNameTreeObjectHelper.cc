#include <qpdf/QPDFNameTreeObjectHelper.hh>

#include <qpdf/NNTree.hh>

namespace
{
    class NameTreeDetails: public NNTreeDetails
    {
      public:
        std::string const&
        itemsKey() const override
        {
            static std::string k("/Names");
            return k;
        }
        bool
        keyValid(QPDFObjectHandle oh) const override
        {
            return oh.isString();
        }
        int
        compareKeys(QPDFObjectHandle a, QPDFObjectHandle b) const override
        {
            if (!(keyValid(a) && keyValid(b))) {
                // We don't call this without calling keyValid first
                throw std::logic_error("comparing invalid keys");
            }
            auto as = a.getUTF8Value();
            auto bs = b.getUTF8Value();
            return ((as < bs) ? -1 : (as > bs) ? 1 : 0);
        }
    };
} // namespace

static NameTreeDetails name_tree_details;

QPDFNameTreeObjectHelper::~QPDFNameTreeObjectHelper()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer. For this specific class, see github issue
    // #745.
}

QPDFNameTreeObjectHelper::Members::Members(QPDFObjectHandle& oh, QPDF& q, bool auto_repair) :
    impl(std::make_shared<NNTreeImpl>(name_tree_details, q, oh, auto_repair))
{
}

QPDFNameTreeObjectHelper::QPDFNameTreeObjectHelper(QPDFObjectHandle oh, QPDF& q, bool auto_repair) :
    QPDFObjectHelper(oh),
    m(new Members(oh, q, auto_repair))
{
}

QPDFNameTreeObjectHelper
QPDFNameTreeObjectHelper::newEmpty(QPDF& qpdf, bool auto_repair)
{
    return QPDFNameTreeObjectHelper(
        qpdf.makeIndirectObject("<< /Names [] >>"_qpdf), qpdf, auto_repair);
}

QPDFNameTreeObjectHelper::iterator::iterator(std::shared_ptr<NNTreeIterator> const& i) :
    impl(i)
{
}

bool
QPDFNameTreeObjectHelper::iterator::valid() const
{
    return impl->valid();
}

QPDFNameTreeObjectHelper::iterator&
QPDFNameTreeObjectHelper::iterator::operator++()
{
    ++(*impl);
    updateIValue();
    return *this;
}

QPDFNameTreeObjectHelper::iterator&
QPDFNameTreeObjectHelper::iterator::operator--()
{
    --(*impl);
    updateIValue();
    return *this;
}

void
QPDFNameTreeObjectHelper::iterator::updateIValue()
{
    if (impl->valid()) {
        auto p = *impl;
        this->ivalue.first = p->first.getUTF8Value();
        this->ivalue.second = p->second;
    } else {
        this->ivalue.first = "";
        this->ivalue.second = QPDFObjectHandle();
    }
}

QPDFNameTreeObjectHelper::iterator::reference
QPDFNameTreeObjectHelper::iterator::operator*()
{
    updateIValue();
    return this->ivalue;
}

QPDFNameTreeObjectHelper::iterator::pointer
QPDFNameTreeObjectHelper::iterator::operator->()
{
    updateIValue();
    return &this->ivalue;
}

bool
QPDFNameTreeObjectHelper::iterator::operator==(iterator const& other) const
{
    return *(impl) == *(other.impl);
}

void
QPDFNameTreeObjectHelper::iterator::insertAfter(std::string const& key, QPDFObjectHandle value)
{
    impl->insertAfter(QPDFObjectHandle::newUnicodeString(key), value);
    updateIValue();
}

void
QPDFNameTreeObjectHelper::iterator::remove()
{
    impl->remove();
    updateIValue();
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::begin() const
{
    return iterator(std::make_shared<NNTreeIterator>(m->impl->begin()));
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::end() const
{
    return iterator(std::make_shared<NNTreeIterator>(m->impl->end()));
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::last() const
{
    return iterator(std::make_shared<NNTreeIterator>(m->impl->last()));
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::find(std::string const& key, bool return_prev_if_not_found)
{
    auto i = m->impl->find(QPDFObjectHandle::newUnicodeString(key), return_prev_if_not_found);
    return iterator(std::make_shared<NNTreeIterator>(i));
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::insert(std::string const& key, QPDFObjectHandle value)
{
    auto i = m->impl->insert(QPDFObjectHandle::newUnicodeString(key), value);
    return iterator(std::make_shared<NNTreeIterator>(i));
}

bool
QPDFNameTreeObjectHelper::remove(std::string const& key, QPDFObjectHandle* value)
{
    return m->impl->remove(QPDFObjectHandle::newUnicodeString(key), value);
}

bool
QPDFNameTreeObjectHelper::hasName(std::string const& name)
{
    auto i = find(name);
    return (i != end());
}

bool
QPDFNameTreeObjectHelper::findObject(std::string const& name, QPDFObjectHandle& oh)
{
    auto i = find(name);
    if (i == end()) {
        return false;
    }
    oh = i->second;
    return true;
}

void
QPDFNameTreeObjectHelper::setSplitThreshold(int t)
{
    m->impl->setSplitThreshold(t);
}

std::map<std::string, QPDFObjectHandle>
QPDFNameTreeObjectHelper::getAsMap() const
{
    std::map<std::string, QPDFObjectHandle> result;
    result.insert(begin(), end());
    return result;
}

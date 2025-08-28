#include <qpdf/QPDFNameTreeObjectHelper.hh>

#include <qpdf/NNTree.hh>

QPDFNameTreeObjectHelper::~QPDFNameTreeObjectHelper() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer. For this specific
    // class, see github issue #745.
}

QPDFNameTreeObjectHelper::Members::Members(
    QPDFObjectHandle& oh,
    QPDF& q,
    std::function<bool(QPDFObjectHandle const&)> value_validator,
    bool auto_repair) :
    impl(std::make_shared<NNTreeImpl>(q, oh, ::ot_string, value_validator, auto_repair))
{
}

QPDFNameTreeObjectHelper::QPDFNameTreeObjectHelper(QPDFObjectHandle oh, QPDF& q, bool auto_repair) :
    QPDFObjectHelper(oh),
    m(new Members(
        oh, q, [](QPDFObjectHandle const& o) -> bool { return static_cast<bool>(o); }, auto_repair))
{
}

QPDFNameTreeObjectHelper::QPDFNameTreeObjectHelper(
    QPDFObjectHandle oh,
    QPDF& q,
    std::function<bool(QPDFObjectHandle const&)> value_validator,
    bool auto_repair) :
    QPDFObjectHelper(oh),
    m(new Members(oh, q, value_validator, auto_repair))
{
}

QPDFNameTreeObjectHelper
QPDFNameTreeObjectHelper::newEmpty(QPDF& qpdf, bool auto_repair)
{
    return {qpdf.makeIndirectObject("<< /Names [] >>"_qpdf), qpdf, auto_repair};
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
        ivalue.first = p->first.getUTF8Value();
        ivalue.second = p->second;
    } else {
        ivalue.first = "";
        ivalue.second = QPDFObjectHandle();
    }
}

QPDFNameTreeObjectHelper::iterator::reference
QPDFNameTreeObjectHelper::iterator::operator*()
{
    updateIValue();
    return ivalue;
}

QPDFNameTreeObjectHelper::iterator::pointer
QPDFNameTreeObjectHelper::iterator::operator->()
{
    updateIValue();
    return &ivalue;
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
    return {std::make_shared<NNTreeIterator>(m->impl->begin())};
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::end() const
{
    return {std::make_shared<NNTreeIterator>(m->impl->end())};
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::last() const
{
    return {std::make_shared<NNTreeIterator>(m->impl->last())};
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::find(std::string const& key, bool return_prev_if_not_found)
{
    auto i = m->impl->find(QPDFObjectHandle::newUnicodeString(key), return_prev_if_not_found);
    return {std::make_shared<NNTreeIterator>(i)};
}

QPDFNameTreeObjectHelper::iterator
QPDFNameTreeObjectHelper::insert(std::string const& key, QPDFObjectHandle value)
{
    auto i = m->impl->insert(QPDFObjectHandle::newUnicodeString(key), value);
    return {std::make_shared<NNTreeIterator>(i)};
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

bool
QPDFNameTreeObjectHelper::validate(bool repair)
{
    return m->impl->validate(repair);
}

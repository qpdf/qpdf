#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/NNTree.hh>

class NameTreeDetails: public NNTreeDetails
{
  public:
    virtual std::string const& itemsKey() const override
    {
        static std::string k("/Names");
        return k;
    }
    virtual bool keyValid(QPDFObjectHandle oh) const override
    {
        return oh.isString();
    }
    virtual int compareKeys(
        QPDFObjectHandle a, QPDFObjectHandle b) const override
    {
        if (! (keyValid(a) && keyValid(b)))
        {
            // We don't call this without calling keyValid first
            throw std::logic_error("comparing invalid keys");
        }
        auto as = a.getUTF8Value();
        auto bs = b.getUTF8Value();
        return ((as < bs) ? -1 : (as > bs) ? 1 : 0);
    }
};

static NameTreeDetails name_tree_details;

QPDFNameTreeObjectHelper::Members::~Members()
{
}

QPDFNameTreeObjectHelper::Members::Members(QPDFObjectHandle& oh) :
    impl(std::make_shared<NNTreeImpl>(name_tree_details, nullptr, oh, false))
{
}

QPDFNameTreeObjectHelper::QPDFNameTreeObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members(oh))
{
}

QPDFNameTreeObjectHelper::~QPDFNameTreeObjectHelper()
{
}

bool
QPDFNameTreeObjectHelper::hasName(std::string const& name)
{
    auto i = this->m->impl->find(QPDFObjectHandle::newUnicodeString(name));
    return (i != this->m->impl->end());
}

bool
QPDFNameTreeObjectHelper::findObject(
    std::string const& name, QPDFObjectHandle& oh)
{
    auto i = this->m->impl->find(QPDFObjectHandle::newUnicodeString(name));
    if (i == this->m->impl->end())
    {
        return false;
    }
    oh = (*i).second;
    return true;
}

std::map<std::string, QPDFObjectHandle>
QPDFNameTreeObjectHelper::getAsMap() const
{
    std::map<std::string, QPDFObjectHandle> result;
    for (auto i: *(this->m->impl))
    {
        result.insert(
            std::make_pair(i.first.getUTF8Value(),
                           i.second));
    }
    return result;
}

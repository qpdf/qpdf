#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/NNTree.hh>

class NumberTreeDetails: public NNTreeDetails
{
  public:
    virtual std::string const& itemsKey() const override
    {
        static std::string k("/Nums");
        return k;
    }
    virtual bool keyValid(QPDFObjectHandle oh) const override
    {
        return oh.isInteger();
    }
    virtual int compareKeys(
        QPDFObjectHandle a, QPDFObjectHandle b) const override
    {
        if (! (keyValid(a) && keyValid(b)))
        {
            // We don't call this without calling keyValid first
            throw std::logic_error("comparing invalid keys");
        }
        auto as = a.getIntValue();
        auto bs = b.getIntValue();
        return ((as < bs) ? -1 : (as > bs) ? 1 : 0);
    }
};

static NumberTreeDetails number_tree_details;

QPDFNumberTreeObjectHelper::Members::~Members()
{
}

QPDFNumberTreeObjectHelper::Members::Members(QPDFObjectHandle& oh) :
    impl(std::make_shared<NNTreeImpl>(number_tree_details, nullptr, oh, false))
{
}

QPDFNumberTreeObjectHelper::QPDFNumberTreeObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members(oh))
{
}

QPDFNumberTreeObjectHelper::numtree_number
QPDFNumberTreeObjectHelper::getMin()
{
    auto i = this->m->impl->begin();
    if (i == this->m->impl->end())
    {
        return 0;
    }
    return (*i).first.getIntValue();
}

QPDFNumberTreeObjectHelper::numtree_number
QPDFNumberTreeObjectHelper::getMax()
{
    auto i = this->m->impl->last();
    if (i == this->m->impl->end())
    {
        return 0;
    }
    return (*i).first.getIntValue();
}

bool
QPDFNumberTreeObjectHelper::hasIndex(numtree_number idx)
{
    auto i = this->m->impl->find(QPDFObjectHandle::newInteger(idx));
    return (i != this->m->impl->end());
}

bool
QPDFNumberTreeObjectHelper::findObject(
    numtree_number idx, QPDFObjectHandle& oh)
{
    auto i = this->m->impl->find(QPDFObjectHandle::newInteger(idx));
    if (i == this->m->impl->end())
    {
        return false;
    }
    oh = (*i).second;
    return true;
}

bool
QPDFNumberTreeObjectHelper::findObjectAtOrBelow(
    numtree_number idx, QPDFObjectHandle& oh,
    numtree_number& offset)
{
    auto i = this->m->impl->find(QPDFObjectHandle::newInteger(idx), true);
    if (i == this->m->impl->end())
    {
        return false;
    }
    oh = (*i).second;
    offset = idx - (*i).first.getIntValue();
    return true;
}

std::map<QPDFNumberTreeObjectHelper::numtree_number, QPDFObjectHandle>
QPDFNumberTreeObjectHelper::getAsMap() const
{
    std::map<numtree_number, QPDFObjectHandle> result;
    for (auto i: *(this->m->impl))
    {
        result.insert(
            std::make_pair(i.first.getIntValue(),
                           i.second));
    }
    return result;
}

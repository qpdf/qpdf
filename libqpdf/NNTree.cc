#include <qpdf/NNTree.hh>
#include <qpdf/QUtil.hh>

#include <exception>

NNTreeIterator::PathElement::PathElement(
    QPDFObjectHandle const& node, int kid_number) :
    node(node),
    kid_number(kid_number)
{
}

QPDFObjectHandle
NNTreeIterator::PathElement::getNextKid(bool backward)
{
    kid_number += backward ? -1 : 1;
    auto kids = node.getKey("/Kids");
    QPDFObjectHandle result;
    if ((kid_number >= 0) && (kid_number < kids.getArrayNItems()))
    {
        result = kids.getArrayItem(kid_number);
    }
    else
    {
        result = QPDFObjectHandle::newNull();
    }
    return result;
}

bool
NNTreeIterator::valid() const
{
    return this->item_number >= 0;
}

void
NNTreeIterator::increment(bool backward)
{
    if (this->item_number < 0)
    {
        throw std::logic_error(
            "attempt made to increment or decrement an invalid"
            " name/number tree iterator");
    }
    this->item_number += backward ? -2 : 2;
    auto items = this->node.getKey(details.itemsKey());
    if ((this->item_number < 0) ||
        (this->item_number >= items.getArrayNItems()))
    {
        bool found = false;
        setItemNumber(QPDFObjectHandle(), -1);
        while (! (found || this->path.empty()))
        {
            auto& element = this->path.back();
            auto node = element.getNextKid(backward);
            if (node.isNull())
            {
                this->path.pop_back();
            }
            else
            {
                deepen(node, ! backward);
                found = true;
            }
        }
    }
}

NNTreeIterator&
NNTreeIterator::operator++()
{
    increment(false);
    return *this;
}

NNTreeIterator&
NNTreeIterator::operator--()
{
    increment(true);
    return *this;
}

NNTreeIterator::reference
NNTreeIterator::operator*()
{
    if (this->item_number < 0)
    {
        throw std::logic_error(
            "attempt made to dereference an invalid"
            " name/number tree iterator");
    }
    auto items = this->node.getKey(details.itemsKey());
    return std::make_pair(items.getArrayItem(this->item_number),
                          items.getArrayItem(1+this->item_number));
}

bool
NNTreeIterator::operator==(NNTreeIterator const& other) const
{
    if ((this->item_number == -1) && (other.item_number == -1))
    {
        return true;
    }
    if (this->path.size() != other.path.size())
    {
        return false;
    }
    auto tpi = this->path.begin();
    auto opi = other.path.begin();
    while (tpi != this->path.end())
    {
        if ((*tpi).kid_number != (*opi).kid_number)
        {
            return false;
        }
        ++tpi;
        ++opi;
    }
    if (this->item_number != other.item_number)
    {
        return false;
    }
    return true;
}

void
NNTreeIterator::setItemNumber(QPDFObjectHandle const& node, int n)
{
    this->node = node;
    this->item_number = n;
}

void
NNTreeIterator::addPathElement(QPDFObjectHandle const& node,
                               int kid_number)
{
    this->path.push_back(PathElement(node, kid_number));
}

void
NNTreeIterator::deepen(QPDFObjectHandle node, bool first)
{
    std::set<QPDFObjGen> seen;
    while (true)
    {
        if (node.isIndirect())
        {
            auto og = node.getObjGen();
            if (seen.count(og))
            {
                throw std::runtime_error("loop detected");
            }
            seen.insert(og);
        }
        auto kids = node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = node.getKey(details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;
        if (nitems > 0)
        {
            setItemNumber(node, first ? 0 : nitems - 2);
            break;
        }
        else if (nkids > 0)
        {
            int kid_number = first ? 0 : nkids - 1;
            addPathElement(node, kid_number);
            node = kids.getArrayItem(kid_number);
        }
        else
        {
            throw std::runtime_error("node has neither /Kids nor /Names");
        }
    }
}

NNTreeImpl::NNTreeImpl(NNTreeDetails const& details,
                       QPDF* qpdf,
                       QPDFObjectHandle& oh,
                       bool auto_repair) :
    details(details),
    oh(oh)
{
}

NNTreeImpl::iterator
NNTreeImpl::begin()
{
    iterator result(details);
    result.deepen(this->oh, true);
    return result;
}

NNTreeImpl::iterator
NNTreeImpl::end()
{
    return iterator(details);
}

NNTreeImpl::iterator
NNTreeImpl::last()
{
    iterator result(details);
    result.deepen(this->oh, false);
    return result;
}

int
NNTreeImpl::withinLimits(QPDFObjectHandle key, QPDFObjectHandle node)
{
    int result = 0;
    auto limits = node.getKey("/Limits");
    if (limits.isArray() && (limits.getArrayNItems() >= 2) &&
        details.keyValid(limits.getArrayItem(0)) &&
        details.keyValid(limits.getArrayItem(1)))
    {
        if (details.compareKeys(key, limits.getArrayItem(0)) < 0)
        {
            result = -1;
        }
        else if (details.compareKeys(key, limits.getArrayItem(1)) > 0)
        {
            result = 1;
        }
    }
    else
    {
        // The root node has no limits, so consider the item to be in
        // here if there are no limits. This will cause checking lower
        // items.
    }
    return result;
}

int
NNTreeImpl::binarySearch(
    QPDFObjectHandle key, QPDFObjectHandle items,
    int num_items, bool return_prev_if_not_found,
    int (NNTreeImpl::*compare)(QPDFObjectHandle& key,
                             QPDFObjectHandle& node,
                             int item))
{
    int max_idx = 1;
    while (max_idx < num_items)
    {
        max_idx <<= 1;
    }

    int step = max_idx / 2;
    int checks = max_idx;
    int idx = step;
    int found_idx = -1;
    bool found = false;
    bool found_leq = false;
    int status = 0;

    while ((! found) && (checks > 0))
    {
        if (idx < num_items)
        {
            status = (this->*compare)(key, items, idx);
            if (status >= 0)
            {
                found_leq = true;
                found_idx = idx;
            }
        }
        else
        {
            // consider item to be below anything after the top
            status = -1;
        }

        if (status == 0)
        {
            found = true;
        }
        else
        {
            checks >>= 1;
            if (checks > 0)
            {
                step >>= 1;
                if (step == 0)
                {
                    step = 1;
                }

                if (status < 0)
                {
                    idx -= step;
                }
                else
                {
                    idx += step;
                }
            }
        }
    }

    if (found || (found_leq && return_prev_if_not_found))
    {
        return found_idx;
    }
    else
    {
        return -1;
    }
}

int
NNTreeImpl::compareKeyItem(
    QPDFObjectHandle& key, QPDFObjectHandle& items, int idx)
{
    if (! ((items.isArray() && (items.getArrayNItems() > (2 * idx)) &&
            details.keyValid(items.getArrayItem(2 * idx)))))
    {
        throw std::runtime_error("item at index " +
                                 QUtil::int_to_string(2 * idx) +
                                 " is not the right type");
    }
    return details.compareKeys(key, items.getArrayItem(2 * idx));
}

int
NNTreeImpl::compareKeyKid(QPDFObjectHandle& key, QPDFObjectHandle& kids, int idx)
{
    if (! (kids.isArray() && (idx < kids.getArrayNItems()) &&
           kids.getArrayItem(idx).isDictionary()))
    {
        throw std::runtime_error("invalid kid at index " +
                                 QUtil::int_to_string(idx));
    }
    return withinLimits(key, kids.getArrayItem(idx));
}


NNTreeImpl::iterator
NNTreeImpl::find(QPDFObjectHandle key, bool return_prev_if_not_found)
{
    auto first_item = begin();
    auto last_item = end();
    if (first_item.valid() &&
        details.keyValid((*first_item).first) &&
        details.compareKeys(key, (*first_item).first) < 0)
    {
        // Before the first key
        return end();
    }
    else if (last_item.valid() &&
             details.keyValid((*last_item).first) &&
             details.compareKeys(key, (*last_item).first) > 0)
    {
        // After the last key
        if (return_prev_if_not_found)
        {
            return last_item;
        }
        else
        {
            return end();
        }
    }

    std::set<QPDFObjGen> seen;
    auto node = this->oh;
    iterator result(details);

    while (true)
    {
        auto og = node.getObjGen();
        if (seen.count(og))
        {
            throw std::runtime_error("loop detected in find");
        }
        seen.insert(og);

        auto kids = node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = node.getKey(details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;
        if (nitems > 0)
        {
            int idx = binarySearch(
                key, items, nitems / 2, return_prev_if_not_found,
                &NNTreeImpl::compareKeyItem);
            if (idx >= 0)
            {
                result.setItemNumber(node, 2 * idx);
            }
            break;
        }
        else if (nkids > 0)
        {
            int idx = binarySearch(
                key, kids, nkids, true,
                &NNTreeImpl::compareKeyKid);
            if (idx == -1)
            {
                throw std::runtime_error(
                    "unexpected -1 from binary search of kids;"
                    " tree may not be sorted");
            }
            result.addPathElement(node, idx);
            node = kids.getArrayItem(idx);
        }
        else
        {
            throw std::runtime_error("bad node during find");
        }
    }

    return result;
}

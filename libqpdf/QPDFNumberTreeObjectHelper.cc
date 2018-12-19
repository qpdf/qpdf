#include <qpdf/QPDFNumberTreeObjectHelper.hh>

QPDFNumberTreeObjectHelper::Members::~Members()
{
}

QPDFNumberTreeObjectHelper::Members::Members()
{
}

QPDFNumberTreeObjectHelper::QPDFNumberTreeObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members())
{
    updateMap(oh);
}

void
QPDFNumberTreeObjectHelper::updateMap(QPDFObjectHandle oh)
{
    if (this->m->seen.count(oh.getObjGen()))
    {
        return;
    }
    this->m->seen.insert(oh.getObjGen());
    QPDFObjectHandle nums = oh.getKey("/Nums");
    if (nums.isArray())
    {
        size_t nitems = nums.getArrayNItems();
        size_t i = 0;
        while (i < nitems - 1)
        {
            QPDFObjectHandle num = nums.getArrayItem(i);
            if (num.isInteger())
            {
                ++i;
                QPDFObjectHandle obj = nums.getArrayItem(i);
                this->m->entries[num.getIntValue()] = obj;
            }
            ++i;
        }
    }
    QPDFObjectHandle kids = oh.getKey("/Kids");
    if (kids.isArray())
    {
        size_t nitems = kids.getArrayNItems();
        for (size_t i = 0; i < nitems; ++i)
        {
            updateMap(kids.getArrayItem(i));
        }
    }
}


QPDFNumberTreeObjectHelper::numtree_number
QPDFNumberTreeObjectHelper::getMin()
{
    if (this->m->entries.empty())
    {
        return 0;
    }
    // Our map is sorted in reverse.
    return this->m->entries.rbegin()->first;
}

QPDFNumberTreeObjectHelper::numtree_number
QPDFNumberTreeObjectHelper::getMax()
{
    if (this->m->entries.empty())
    {
        return 0;
    }
    // Our map is sorted in reverse.
    return this->m->entries.begin()->first;
}

bool
QPDFNumberTreeObjectHelper::hasIndex(numtree_number idx)
{
    return this->m->entries.count(idx) != 0;
}

bool
QPDFNumberTreeObjectHelper::findObject(
    numtree_number idx, QPDFObjectHandle& oh)
{
    Members::idx_map::iterator i = this->m->entries.find(idx);
    if (i == this->m->entries.end())
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
    Members::idx_map::iterator i = this->m->entries.lower_bound(idx);
    if (i == this->m->entries.end())
    {
        return false;
    }
    oh = (*i).second;
    offset = idx - (*i).first;
    return true;
}

std::map<QPDFNumberTreeObjectHelper::numtree_number, QPDFObjectHandle>
QPDFNumberTreeObjectHelper::getAsMap() const
{
    std::map<numtree_number, QPDFObjectHandle> result;
    for (Members::idx_map::const_iterator iter = this->m->entries.begin();
         iter != this->m->entries.end(); ++iter)
    {
        result[(*iter).first] = (*iter).second;
    }
    return result;
}

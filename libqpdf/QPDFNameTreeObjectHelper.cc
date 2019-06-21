#include <qpdf/QPDFNameTreeObjectHelper.hh>

QPDFNameTreeObjectHelper::Members::~Members()
{
}

QPDFNameTreeObjectHelper::Members::Members()
{
}

QPDFNameTreeObjectHelper::QPDFNameTreeObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members())
{
    updateMap(oh);
}

QPDFNameTreeObjectHelper::~QPDFNameTreeObjectHelper()
{
}

void
QPDFNameTreeObjectHelper::updateMap(QPDFObjectHandle oh)
{
    if (this->m->seen.count(oh.getObjGen()))
    {
        return;
    }
    this->m->seen.insert(oh.getObjGen());
    QPDFObjectHandle names = oh.getKey("/Names");
    if (names.isArray())
    {
        int nitems = names.getArrayNItems();
        int i = 0;
        while (i < nitems - 1)
        {
            QPDFObjectHandle name = names.getArrayItem(i);
            if (name.isString())
            {
                ++i;
                QPDFObjectHandle obj = names.getArrayItem(i);
                this->m->entries[name.getUTF8Value()] = obj;
            }
            ++i;
        }
    }
    QPDFObjectHandle kids = oh.getKey("/Kids");
    if (kids.isArray())
    {
        int nitems = kids.getArrayNItems();
        for (int i = 0; i < nitems; ++i)
        {
            updateMap(kids.getArrayItem(i));
        }
    }
}

bool
QPDFNameTreeObjectHelper::hasName(std::string const& name)
{
    return this->m->entries.count(name) != 0;
}

bool
QPDFNameTreeObjectHelper::findObject(
    std::string const& name, QPDFObjectHandle& oh)
{
    std::map<std::string, QPDFObjectHandle>::iterator i =
        this->m->entries.find(name);
    if (i == this->m->entries.end())
    {
        return false;
    }
    oh = (*i).second;
    return true;
}

std::map<std::string, QPDFObjectHandle>
QPDFNameTreeObjectHelper::getAsMap() const
{
    return this->m->entries;
}

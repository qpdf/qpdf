#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QTC.hh>

QPDFOutlineDocumentHelper::Members::~Members()
{
}

QPDFOutlineDocumentHelper::Members::Members()
{
}

QPDFOutlineDocumentHelper::QPDFOutlineDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(new Members())
{
    QPDFObjectHandle root = qpdf.getRoot();
    if (! root.hasKey("/Outlines"))
    {
        return;
    }
    QPDFObjectHandle outlines = root.getKey("/Outlines");
    if (! (outlines.isDictionary() && outlines.hasKey("/First")))
    {
        return;
    }
    QPDFObjectHandle cur = outlines.getKey("/First");
    while (! cur.isNull())
    {
        this->m->outlines.push_back(
            QPDFOutlineObjectHelper::Accessor::create(cur, *this, 1));
        cur = cur.getKey("/Next");
    }
}

QPDFOutlineDocumentHelper::~QPDFOutlineDocumentHelper()
{
}

bool
QPDFOutlineDocumentHelper::hasOutlines()
{
    return ! this->m->outlines.empty();
}

std::list<PointerHolder<QPDFOutlineObjectHelper> >
QPDFOutlineDocumentHelper::getTopLevelOutlines()
{
    return this->m->outlines;
}

void
QPDFOutlineDocumentHelper::initializeByPage()
{
    std::list<PointerHolder<QPDFOutlineObjectHelper> > queue;
    queue.insert(queue.end(), this->m->outlines.begin(), this->m->outlines.end());

    while (! queue.empty())
    {
        PointerHolder<QPDFOutlineObjectHelper> oh = queue.front();
        queue.pop_front();
        this->m->by_page[oh->getDestPage().getObjGen()].push_back(*oh);
        std::list<PointerHolder<QPDFOutlineObjectHelper> > kids = oh->getKids();
        queue.insert(queue.end(), kids.begin(), kids.end());
    }
}

std::list<QPDFOutlineObjectHelper>
QPDFOutlineDocumentHelper::getOutlinesForPage(QPDFObjGen const& og)
{
    if (this->m->by_page.empty())
    {
        initializeByPage();
    }
    std::list<QPDFOutlineObjectHelper> result;
    if (this->m->by_page.count(og))
    {
        result = this->m->by_page[og];
    }
    return result;
}

QPDFObjectHandle
QPDFOutlineDocumentHelper::resolveNamedDest(QPDFObjectHandle name)
{
    QPDFObjectHandle result;
    if (name.isName())
    {
        if (! this->m->dest_dict.isInitialized())
        {
            this->m->dest_dict = this->qpdf.getRoot().getKey("/Dests");
        }
        if (this->m->dest_dict.isDictionary())
        {
            QTC::TC("qpdf", "QPDFOutlineDocumentHelper name named dest");
            result = this->m->dest_dict.getKey(name.getName());
        }
    }
    else if (name.isString())
    {
        if (0 == this->m->names_dest.getPointer())
        {
            QPDFObjectHandle names = this->qpdf.getRoot().getKey("/Names");
            if (names.isDictionary())
            {
                QPDFObjectHandle dests = names.getKey("/Dests");
                if (dests.isDictionary())
                {
                    this->m->names_dest =
                        new QPDFNameTreeObjectHelper(dests);
                }
            }
        }
        if (this->m->names_dest.getPointer())
        {
            if (this->m->names_dest->findObject(name.getUTF8Value(), result))
            {
                QTC::TC("qpdf", "QPDFOutlineDocumentHelper string named dest");
            }
        }
    }
    if (! result.isInitialized())
    {
        result = QPDFObjectHandle::newNull();
    }
    return result;
}

bool
QPDFOutlineDocumentHelper::checkSeen(QPDFObjGen const& og)
{
    if (this->m->seen.count(og) > 0)
    {
        return true;
    }
    this->m->seen.insert(og);
    return false;
}

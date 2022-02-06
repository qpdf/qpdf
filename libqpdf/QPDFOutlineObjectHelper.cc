#include <qpdf/QPDFOutlineObjectHelper.hh>

#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QTC.hh>

QPDFOutlineObjectHelper::Members::~Members()
{
}

QPDFOutlineObjectHelper::Members::Members(QPDFOutlineDocumentHelper& dh) :
    dh(dh)
{
}

QPDFOutlineObjectHelper::QPDFOutlineObjectHelper(
    QPDFObjectHandle oh, QPDFOutlineDocumentHelper& dh, int depth) :
    QPDFObjectHelper(oh),
    m(new Members(dh))
{
    if (depth > 50)
    {
        // Not exercised in test suite, but was tested manually by
        // temporarily changing max depth to 1.
        return;
    }
    if (QPDFOutlineDocumentHelper::Accessor::checkSeen(
            this->m->dh, this->oh.getObjGen()))
    {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper loop");
        return;
    }

    QPDFObjectHandle cur = oh.getKey("/First");
    while (! cur.isNull())
    {
        QPDFOutlineObjectHelper new_ooh(cur, dh, 1 + depth);
        new_ooh.m->parent = make_pointer_holder<QPDFOutlineObjectHelper>(*this);
        this->m->kids.push_back(new_ooh);
        cur = cur.getKey("/Next");
    }
}

PointerHolder<QPDFOutlineObjectHelper>
QPDFOutlineObjectHelper::getParent()
{
    return this->m->parent;
}

std::vector<QPDFOutlineObjectHelper>
QPDFOutlineObjectHelper::getKids()
{
    return this->m->kids;
}

QPDFObjectHandle
QPDFOutlineObjectHelper::getDest()
{
    QPDFObjectHandle dest;
    QPDFObjectHandle A;
    if (this->oh.hasKey("/Dest"))
    {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper direct dest");
        dest = this->oh.getKey("/Dest");
    }
    else if ((A = this->oh.getKey("/A")).isDictionary() &&
             A.getKey("/S").isName() &&
             (A.getKey("/S").getName() == "/GoTo") &&
             A.hasKey("/D"))
    {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper action dest");
        dest = A.getKey("/D");
    }
    if (! dest.isInitialized())
    {
        dest = QPDFObjectHandle::newNull();
    }

    if (dest.isName() || dest.isString())
    {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper named dest");
        dest = this->m->dh.resolveNamedDest(dest);
    }

    return dest;
}

QPDFObjectHandle
QPDFOutlineObjectHelper::getDestPage()
{
    QPDFObjectHandle dest = getDest();
    if ((dest.isArray()) && (dest.getArrayNItems() > 0))
    {
        return dest.getArrayItem(0);
    }
    return QPDFObjectHandle::newNull();
}

int
QPDFOutlineObjectHelper::getCount()
{
    int count = 0;
    if (this->oh.hasKey("/Count"))
    {
        count = this->oh.getKey("/Count").getIntValueAsInt();
    }
    return count;
}

std::string
QPDFOutlineObjectHelper::getTitle()
{
    std::string result;
    if (this->oh.hasKey("/Title"))
    {
        result = this->oh.getKey("/Title").getUTF8Value();
    }
    return result;
}

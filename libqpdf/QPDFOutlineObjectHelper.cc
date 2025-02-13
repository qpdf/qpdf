#include <qpdf/QPDFOutlineObjectHelper.hh>

#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QTC.hh>

QPDFOutlineObjectHelper::Members::Members(QPDFOutlineDocumentHelper& dh) :
    dh(dh)
{
}

QPDFOutlineObjectHelper::QPDFOutlineObjectHelper(
    QPDFObjectHandle a_oh, QPDFOutlineDocumentHelper& dh, int depth) :
    QPDFObjectHelper(a_oh),
    m(new Members(dh))
{
    if (depth > 50) {
        // Not exercised in test suite, but was tested manually by temporarily changing max depth
        // to 1.
        return;
    }
    if (QPDFOutlineDocumentHelper::Accessor::checkSeen(m->dh, a_oh.getObjGen())) {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper loop");
        return;
    }

    QPDFObjGen::set children;
    QPDFObjectHandle cur = a_oh.getKey("/First");
    while (!cur.isNull() && cur.isIndirect() && children.add(cur)) {
        QPDFOutlineObjectHelper new_ooh(cur, dh, 1 + depth);
        new_ooh.m->parent = std::make_shared<QPDFOutlineObjectHelper>(*this);
        m->kids.push_back(new_ooh);
        cur = cur.getKey("/Next");
    }
}

std::shared_ptr<QPDFOutlineObjectHelper>
QPDFOutlineObjectHelper::getParent()
{
    return m->parent;
}

std::vector<QPDFOutlineObjectHelper>
QPDFOutlineObjectHelper::getKids()
{
    return m->kids;
}

QPDFObjectHandle
QPDFOutlineObjectHelper::getDest()
{
    QPDFObjectHandle dest;
    QPDFObjectHandle A;
    if (oh().hasKey("/Dest")) {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper direct dest");
        dest = oh().getKey("/Dest");
    } else if (
        (A = oh().getKey("/A")).isDictionary() && A.getKey("/S").isName() &&
        (A.getKey("/S").getName() == "/GoTo") && A.hasKey("/D")) {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper action dest");
        dest = A.getKey("/D");
    }
    if (!dest) {
        return QPDFObjectHandle::newNull();
    }

    if (dest.isName() || dest.isString()) {
        QTC::TC("qpdf", "QPDFOutlineObjectHelper named dest");
        dest = m->dh.resolveNamedDest(dest);
    }

    return dest;
}

QPDFObjectHandle
QPDFOutlineObjectHelper::getDestPage()
{
    QPDFObjectHandle dest = getDest();
    if ((dest.isArray()) && (dest.getArrayNItems() > 0)) {
        return dest.getArrayItem(0);
    }
    return QPDFObjectHandle::newNull();
}

int
QPDFOutlineObjectHelper::getCount()
{
    int count = 0;
    if (oh().hasKey("/Count")) {
        count = oh().getKey("/Count").getIntValueAsInt();
    }
    return count;
}

std::string
QPDFOutlineObjectHelper::getTitle()
{
    std::string result;
    if (oh().hasKey("/Title")) {
        result = oh().getKey("/Title").getUTF8Value();
    }
    return result;
}

#include <qpdf/QPDFOutlineObjectHelper.hh>

#include <qpdf/QPDFObjectHandle_private.hh>
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
        a_oh.warn("Loop detected loop in /Outlines tree");
        return;
    }

    QPDFObjGen::set children;
    QPDFObjectHandle cur = a_oh.getKey("/First");
    while (!cur.null() && cur.isIndirect()) {
        if (!children.add(cur)) {
            cur.warn("Loop detected loop in /Outlines tree");
            break;
        }
        QPDFOutlineObjectHelper new_ooh(cur, dh, 1 + depth);
        new_ooh.m->parent = std::make_shared<QPDFOutlineObjectHelper>(*this);
        m->kids.emplace_back(new_ooh);
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
    auto dest = get("/Dest");
    if (dest.null()) {
        auto const& A = get("/A");
        if (Name(A["/S"]) == "/GoTo") {
            dest = A["/D"];
        }
    }
    if (dest.null()) {
        return QPDFObjectHandle::newNull();
    }
    if (dest.isName() || dest.isString()) {
        return m->dh.resolveNamedDest(dest);
    }
    return dest;
}

QPDFObjectHandle
QPDFOutlineObjectHelper::getDestPage()
{
    QPDFObjectHandle dest = getDest();
    if (!dest.empty() && dest.isArray()) {
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

#include <qpdf/QPDFOutlineDocumentHelper.hh>

#include <qpdf/QTC.hh>

class QPDFOutlineDocumentHelper::Members
{
  public:
    Members() = default;
    Members(Members const&) = delete;
    ~Members() = default;

    std::vector<QPDFOutlineObjectHelper> outlines;
    QPDFObjGen::set seen;
    QPDFObjectHandle dest_dict;
    std::unique_ptr<QPDFNameTreeObjectHelper> names_dest;
    std::map<QPDFObjGen, std::vector<QPDFOutlineObjectHelper>> by_page;
};

bool
QPDFOutlineDocumentHelper::Accessor::checkSeen(QPDFOutlineDocumentHelper& dh, QPDFObjGen og)
{
    return !dh.m->seen.add(og);
}

QPDFOutlineDocumentHelper::QPDFOutlineDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(std::make_shared<Members>())
{
    QPDFObjectHandle root = qpdf.getRoot();
    if (!root.hasKey("/Outlines")) {
        return;
    }
    auto outlines = root.getKey("/Outlines");
    if (!(outlines.isDictionary() && outlines.hasKey("/First"))) {
        return;
    }
    QPDFObjectHandle cur = outlines.getKey("/First");
    QPDFObjGen::set seen;
    while (!cur.isNull() && seen.add(cur)) {
        m->outlines.emplace_back(QPDFOutlineObjectHelper::Accessor::create(cur, *this, 1));
        cur = cur.getKey("/Next");
    }
}

bool
QPDFOutlineDocumentHelper::hasOutlines()
{
    return !m->outlines.empty();
}

std::vector<QPDFOutlineObjectHelper>
QPDFOutlineDocumentHelper::getTopLevelOutlines()
{
    return m->outlines;
}

void
QPDFOutlineDocumentHelper::initializeByPage()
{
    std::list<QPDFOutlineObjectHelper> queue;
    queue.insert(queue.end(), m->outlines.begin(), m->outlines.end());

    while (!queue.empty()) {
        QPDFOutlineObjectHelper oh = queue.front();
        queue.pop_front();
        m->by_page[oh.getDestPage().getObjGen()].push_back(oh);
        std::vector<QPDFOutlineObjectHelper> kids = oh.getKids();
        queue.insert(queue.end(), kids.begin(), kids.end());
    }
}

std::vector<QPDFOutlineObjectHelper>
QPDFOutlineDocumentHelper::getOutlinesForPage(QPDFObjGen og)
{
    if (m->by_page.empty()) {
        initializeByPage();
    }
    if (m->by_page.contains(og)) {
        return m->by_page[og];
    }
    return {};
}

QPDFObjectHandle
QPDFOutlineDocumentHelper::resolveNamedDest(QPDFObjectHandle name)
{
    QPDFObjectHandle result;
    if (name.isName()) {
        if (!m->dest_dict) {
            m->dest_dict = qpdf.getRoot().getKey("/Dests");
        }
        result = m->dest_dict.getKeyIfDict(name.getName());
    } else if (name.isString()) {
        if (!m->names_dest) {
            auto dests = qpdf.getRoot().getKey("/Names").getKeyIfDict("/Dests");
            if (dests.isDictionary()) {
                m->names_dest = std::make_unique<QPDFNameTreeObjectHelper>(dests, qpdf);
            }
        }
        if (m->names_dest) {
            if (m->names_dest->findObject(name.getUTF8Value(), result)) {
                QTC::TC("qpdf", "QPDFOutlineDocumentHelper string named dest");
            }
        }
    }
    if (!result) {
        return QPDFObjectHandle::newNull();
    }
    if (result.isDictionary()) {
        return result.getKey("/D");
    }
    return result;
}

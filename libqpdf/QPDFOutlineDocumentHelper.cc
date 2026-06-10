#include <qpdf/QPDFNamedDestinationDocumentHelper.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>

class QPDFOutlineDocumentHelper::Members
{
  public:
    Members() = default;
    Members(Members const&) = delete;
    ~Members() = default;

    std::vector<QPDFOutlineObjectHelper> outlines;
    QPDFObjGen::set seen;
    std::map<QPDFObjGen, std::vector<QPDFOutlineObjectHelper>> by_page;
    std::unique_ptr<QPDFNamedDestinationDocumentHelper> named_dest_document_helper;
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
    validate();
}

QPDFOutlineDocumentHelper&
QPDFOutlineDocumentHelper::get(QPDF& qpdf)
{
    return qpdf.doc().outlines();
}

void
QPDFOutlineDocumentHelper::validate(bool repair)
{
    m->outlines.clear();
    m->by_page.clear();
    m->named_dest_document_helper = nullptr;

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
    while (!cur.null()) {
        if (!seen.add(cur)) {
            cur.warn("Loop detected loop in /Outlines tree");
            return;
        }
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
    if (!m->named_dest_document_helper) {
        m->named_dest_document_helper = std::make_unique<QPDFNamedDestinationDocumentHelper>(qpdf);
    }
    auto named_dest_object_helper = m->named_dest_document_helper->lookup(name);
    if (named_dest_object_helper.isNull()) {
        return QPDFObjectHandle::newNull();
    }
    auto explicit_dest_helper = named_dest_object_helper.unwrap();
    return explicit_dest_helper.getExplicitArray();
}

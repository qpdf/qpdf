#include <qpdf/QPDF.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

// In support of page manipulation APIs, these methods internally maintain state about pages in a
// pair of data structures: all_pages, which is a vector of page objects, and pageobj_to_pages_pos,
// which maps a page object to its position in the all_pages array. Unfortunately, the getAllPages()
// method returns a const reference to all_pages and has been in the public API long before the
// introduction of mutation APIs, so we're pretty much stuck with it. Anyway, there are lots of
// calls to it in the library, so the efficiency of having it cached is probably worth keeping it.
// At one point, I had partially implemented a helper class specifically for the pages tree, but
// once you work in all the logic that handles repairing the /Type keys of page tree nodes (both
// /Pages and /Page) and deal with duplicate pages, it's just as complex and less efficient than
// what's here. So, in spite of the fact that a const reference is returned, the current code is
// fine and does not need to be replaced. A partial implementation of QPDFPagesTree is in github in
// attic in case there is ever a reason to resurrect it. There are additional notes in
// README-maintainer, which also refers to this comment.

// The goal of this code is to ensure that the all_pages vector, which users may have a reference
// to, and the pageobj_to_pages_pos map, which users will not have access to, remain consistent
// outside of any call to the library.  As long as users only touch the /Pages structure through
// page-specific API calls, they never have to worry about anything, and this will also stay
// consistent.  If a user touches anything about the /Pages structure outside of these calls (such
// as by directly looking up and manipulating the underlying objects), they can call
// updatePagesCache() to bring things back in sync.

// If the user doesn't ever use the page manipulation APIs, then qpdf leaves the /Pages structure
// alone.  If the user does use the APIs, then we push all inheritable objects down and flatten the
// /Pages tree.  This makes it easier for us to keep /Pages, all_pages, and pageobj_to_pages_pos
// internally consistent at all times.

// Responsibility for keeping all_pages, pageobj_to_pages_pos, and the Pages structure consistent
// should remain in as few places as possible.  As of initial writing, only flattenPagesTree,
// insertPage, and removePage, along with methods they call, are concerned with it.  Everything else
// goes through one of those methods.

std::vector<QPDFObjectHandle> const&
QPDF::getAllPages()
{
    // Note that pushInheritedAttributesToPage may also be used to initialize m->all_pages.
    if (m->all_pages.empty() && !m->invalid_page_found) {
        m->ever_called_get_all_pages = true;
        QPDFObjGen::set visited;
        QPDFObjGen::set seen;
        QPDFObjectHandle pages = getRoot().getKey("/Pages");
        bool warned = false;
        bool changed_pages = false;
        while (pages.isDictionary() && pages.hasKey("/Parent")) {
            if (!seen.add(pages)) {
                // loop -- will be detected again and reported later
                break;
            }
            // Files have been found in the wild where /Pages in the catalog points to the first
            // page. Try to work around this and similar cases with this heuristic.
            if (!warned) {
                getRoot().warnIfPossible("document page tree root (root -> /Pages) doesn't point"
                                         " to the root of the page tree; attempting to correct");
                warned = true;
            }
            changed_pages = true;
            pages = pages.getKey("/Parent");
        }
        if (changed_pages) {
            getRoot().replaceKey("/Pages", pages);
        }
        seen.clear();
        if (!pages.hasKey("/Kids")) {
            // Ensure we actually found a /Pages object.
            throw QPDFExc(
                qpdf_e_pages, m->file->getName(), "", 0, "root of pages tree has no /Kids array");
        }
        try {
            getAllPagesInternal(pages, visited, seen, false);
        } catch (...) {
            m->all_pages.clear();
            m->invalid_page_found = false;
            throw;
        }
        if (m->invalid_page_found) {
            flattenPagesTree();
            m->invalid_page_found = false;
        }
    }
    return m->all_pages;
}

void
QPDF::getAllPagesInternal(
    QPDFObjectHandle cur_node, QPDFObjGen::set& visited, QPDFObjGen::set& seen, bool media_box)
{
    if (!visited.add(cur_node)) {
        throw QPDFExc(
            qpdf_e_pages,
            m->file->getName(),
            m->last_object_description,
            0,
            "Loop detected in /Pages structure (getAllPages)");
    }
    if (!cur_node.isDictionaryOfType("/Pages")) {
        cur_node.warnIfPossible("/Type key should be /Pages but is not; overriding");
        cur_node.replaceKey("/Type", "/Pages"_qpdf);
    }
    if (!media_box) {
        media_box = cur_node.getKey("/MediaBox").isRectangle();
        QTC::TC("qpdf", "QPDF inherit mediabox", media_box ? 0 : 1);
    }
    auto kids = cur_node.getKey("/Kids");
    int n = kids.getArrayNItems();
    for (int i = 0; i < n; ++i) {
        auto kid = kids.getArrayItem(i);
        if (!kid.isDictionary()) {
            kid.warnIfPossible("Pages tree includes non-dictionary object; ignoring");
            m->invalid_page_found = true;
            continue;
        }
        if (kid.hasKey("/Kids")) {
            getAllPagesInternal(kid, visited, seen, media_box);
        } else {
            if (!media_box && !kid.getKey("/MediaBox").isRectangle()) {
                QTC::TC("qpdf", "QPDF missing mediabox");
                kid.warnIfPossible(
                    "kid " + std::to_string(i) +
                    " (from 0) MediaBox is undefined; setting to letter / ANSI A");
                kid.replaceKey(
                    "/MediaBox",
                    QPDFObjectHandle::newArray(QPDFObjectHandle::Rectangle(0, 0, 612, 792)));
            }
            if (!kid.isIndirect()) {
                QTC::TC("qpdf", "QPDF handle direct page object");
                cur_node.warnIfPossible(
                    "kid " + std::to_string(i) + " (from 0) is direct; converting to indirect");
                kid = makeIndirectObject(kid);
                kids.setArrayItem(i, kid);
            } else if (!seen.add(kid)) {
                // Make a copy of the page. This does the same as shallowCopyPage in
                // QPDFPageObjectHelper.
                QTC::TC("qpdf", "QPDF resolve duplicated page object");
                cur_node.warnIfPossible(
                    "kid " + std::to_string(i) +
                    " (from 0) appears more than once in the pages tree;"
                    " creating a new page object as a copy");
                kid = makeIndirectObject(QPDFObjectHandle(kid).shallowCopy());
                seen.add(kid);
                kids.setArrayItem(i, kid);
            }
            if (!kid.isDictionaryOfType("/Page")) {
                kid.warnIfPossible("/Type key should be /Page but is not; overriding");
                kid.replaceKey("/Type", "/Page"_qpdf);
            }
            m->all_pages.push_back(kid);
        }
    }
}

void
QPDF::updateAllPagesCache()
{
    // Force regeneration of the pages cache.  We force immediate recalculation of all_pages since
    // users may have references to it that they got from calls to getAllPages().  We can defer
    // recalculation of pageobj_to_pages_pos until needed.
    QTC::TC("qpdf", "QPDF updateAllPagesCache");
    m->all_pages.clear();
    m->pageobj_to_pages_pos.clear();
    m->pushed_inherited_attributes_to_pages = false;
    getAllPages();
}

void
QPDF::flattenPagesTree()
{
    // If not already done, flatten the /Pages structure and initialize pageobj_to_pages_pos.

    if (!m->pageobj_to_pages_pos.empty()) {
        return;
    }

    // Push inherited objects down to the /Page level.  As a side effect m->all_pages will also be
    // generated.
    pushInheritedAttributesToPage(true, true);

    QPDFObjectHandle pages = getRoot().getKey("/Pages");

    size_t const len = m->all_pages.size();
    for (size_t pos = 0; pos < len; ++pos) {
        // Populate pageobj_to_pages_pos and fix parent pointer. There should be no duplicates at
        // this point because pushInheritedAttributesToPage calls getAllPages which resolves
        // duplicates.
        insertPageobjToPage(m->all_pages.at(pos), toI(pos), true);
        m->all_pages.at(pos).replaceKey("/Parent", pages);
    }

    pages.replaceKey("/Kids", QPDFObjectHandle::newArray(m->all_pages));
    // /Count has not changed
    if (pages.getKey("/Count").getUIntValue() != len) {
        if (m->invalid_page_found && pages.getKey("/Count").getUIntValue() > len) {
            pages.replaceKey("/Count", QPDFObjectHandle::newInteger(toI(len)));
        } else {
            throw std::runtime_error("/Count is wrong after flattening pages tree");
        }
    }
}

void
QPDF::insertPageobjToPage(QPDFObjectHandle const& obj, int pos, bool check_duplicate)
{
    QPDFObjGen og(obj.getObjGen());
    if (check_duplicate) {
        if (!m->pageobj_to_pages_pos.insert(std::make_pair(og, pos)).second) {
            // The library never calls insertPageobjToPage in a way that causes this to happen.
            setLastObjectDescription("page " + std::to_string(pos) + " (numbered from zero)", og);
            throw QPDFExc(
                qpdf_e_pages,
                m->file->getName(),
                m->last_object_description,
                0,
                "duplicate page reference found; this would cause loss of data");
        }
    } else {
        m->pageobj_to_pages_pos[og] = pos;
    }
}

void
QPDF::insertPage(QPDFObjectHandle newpage, int pos)
{
    // pos is numbered from 0, so pos = 0 inserts at the beginning and pos = npages adds to the end.

    flattenPagesTree();

    if (!newpage.isIndirect()) {
        QTC::TC("qpdf", "QPDF insert non-indirect page");
        newpage = makeIndirectObject(newpage);
    } else if (newpage.getOwningQPDF() != this) {
        QTC::TC("qpdf", "QPDF insert foreign page");
        newpage.getQPDF().pushInheritedAttributesToPage();
        newpage = copyForeignObject(newpage);
    } else {
        QTC::TC("qpdf", "QPDF insert indirect page");
    }

    if ((pos < 0) || (toS(pos) > m->all_pages.size())) {
        throw std::runtime_error("QPDF::insertPage called with pos out of range");
    }

    QTC::TC(
        "qpdf",
        "QPDF insert page",
        (pos == 0) ? 0 :                            // insert at beginning
            (pos == toI(m->all_pages.size())) ? 1   // at end
                                              : 2); // insert in middle

    auto og = newpage.getObjGen();
    if (m->pageobj_to_pages_pos.count(og)) {
        QTC::TC("qpdf", "QPDF resolve duplicated page in insert");
        newpage = makeIndirectObject(QPDFObjectHandle(newpage).shallowCopy());
    }

    QPDFObjectHandle pages = getRoot().getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    newpage.replaceKey("/Parent", pages);
    kids.insertItem(pos, newpage);
    int npages = kids.getArrayNItems();
    pages.replaceKey("/Count", QPDFObjectHandle::newInteger(npages));
    m->all_pages.insert(m->all_pages.begin() + pos, newpage);
    for (int i = pos + 1; i < npages; ++i) {
        insertPageobjToPage(m->all_pages.at(toS(i)), i, false);
    }
    insertPageobjToPage(newpage, pos, true);
}

void
QPDF::removePage(QPDFObjectHandle page)
{
    int pos = findPage(page); // also ensures flat /Pages
    QTC::TC(
        "qpdf",
        "QPDF remove page",
        (pos == 0) ? 0 :                                // remove at beginning
            (pos == toI(m->all_pages.size() - 1)) ? 1   // end
                                                  : 2); // remove in middle

    QPDFObjectHandle pages = getRoot().getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    kids.eraseItem(pos);
    int npages = kids.getArrayNItems();
    pages.replaceKey("/Count", QPDFObjectHandle::newInteger(npages));
    m->all_pages.erase(m->all_pages.begin() + pos);
    m->pageobj_to_pages_pos.erase(page.getObjGen());
    for (int i = pos; i < npages; ++i) {
        insertPageobjToPage(m->all_pages.at(toS(i)), i, false);
    }
}

void
QPDF::addPageAt(QPDFObjectHandle newpage, bool before, QPDFObjectHandle refpage)
{
    int refpos = findPage(refpage);
    if (!before) {
        ++refpos;
    }
    insertPage(newpage, refpos);
}

void
QPDF::addPage(QPDFObjectHandle newpage, bool first)
{
    if (first) {
        insertPage(newpage, 0);
    } else {
        insertPage(newpage, getRoot().getKey("/Pages").getKey("/Count").getIntValueAsInt());
    }
}

int
QPDF::findPage(QPDFObjectHandle& page)
{
    return findPage(page.getObjGen());
}

int
QPDF::findPage(QPDFObjGen const& og)
{
    flattenPagesTree();
    auto it = m->pageobj_to_pages_pos.find(og);
    if (it == m->pageobj_to_pages_pos.end()) {
        QTC::TC("qpdf", "QPDF_pages findPage not found");
        setLastObjectDescription("page object", og);
        throw QPDFExc(
            qpdf_e_pages,
            m->file->getName(),
            m->last_object_description,
            0,
            "page object not referenced in /Pages tree");
    }
    return (*it).second;
}

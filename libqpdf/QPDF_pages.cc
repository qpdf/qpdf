#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDF_private.hh>

#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

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

using Pages = QPDF::Doc::Pages;

std::vector<QPDFObjectHandle> const&
QPDF::getAllPages()
{
    return m->pages.all();
}

std::vector<QPDFObjectHandle> const&
Pages::cache()
{
    // Note that pushInheritedAttributesToPage may also be used to initialize m->all_pages.
    if (all_pages.empty() && !invalid_page_found) {
        ever_called_get_all_pages_ = true;
        auto root = qpdf.getRoot();
        QPDFObjGen::set visited;
        QPDFObjGen::set seen;
        QPDFObjectHandle pages = root.getKey("/Pages");
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
                root.warn(
                    "document page tree root (root -> /Pages) doesn't point"
                    " to the root of the page tree; attempting to correct");
                warned = true;
            }
            changed_pages = true;
            pages = pages.getKey("/Parent");
        }
        if (changed_pages) {
            root.replaceKey("/Pages", pages);
        }
        seen.clear();
        if (!pages.hasKey("/Kids")) {
            // Ensure we actually found a /Pages object.
            throw QPDFExc(
                qpdf_e_pages, m->file->getName(), "", 0, "root of pages tree has no /Kids array");
        }
        try {
            getAllPagesInternal(pages, visited, seen, false, false);
        } catch (...) {
            all_pages.clear();
            invalid_page_found = false;
            throw;
        }
        if (invalid_page_found) {
            flattenPagesTree();
            invalid_page_found = false;
        }
    }
    return all_pages;
}

void
Pages::getAllPagesInternal(
    QPDFObjectHandle cur_node,
    QPDFObjGen::set& visited,
    QPDFObjGen::set& seen,
    bool media_box,
    bool resources)
{
    if (!visited.add(cur_node)) {
        throw QPDFExc(
            qpdf_e_pages,
            m->file->getName(),
            "object " + cur_node.getObjGen().unparse(' '),
            0,
            "Loop detected in /Pages structure (getAllPages)");
    }
    if (!cur_node.isDictionaryOfType("/Pages")) {
        // During fuzzing files were encountered where the root object appeared in the pages tree.
        // Unconditionally setting the /Type to /Pages could cause problems, but trying to
        // accommodate the possibility may be excessive.
        cur_node.warn("/Type key should be /Pages but is not; overriding");
        cur_node.replaceKey("/Type", "/Pages"_qpdf);
    }
    if (!media_box) {
        media_box = cur_node.getKey("/MediaBox").isRectangle();
        QTC::TC("qpdf", "QPDF inherit mediabox", media_box ? 0 : 1);
    }
    if (!resources) {
        resources = cur_node.getKey("/Resources").isDictionary();
    }
    auto kids = cur_node.getKey("/Kids");
    if (!visited.add(kids)) {
        throw QPDFExc(
            qpdf_e_pages,
            m->file->getName(),
            "object " + cur_node.getObjGen().unparse(' '),
            0,
            "Loop detected in /Pages structure (getAllPages)");
    }
    int i = -1;
    for (auto& kid: kids.as_array()) {
        ++i;
        int errors = 0;

        if (!kid.isDictionary()) {
            kid.warn("Pages tree includes non-dictionary object; ignoring");
            invalid_page_found = true;
            continue;
        }
        if (!kid.isIndirect()) {
            cur_node.warn(
                "kid " + std::to_string(i) + " (from 0) is direct; converting to indirect");
            kid = qpdf.makeIndirectObject(kid);
            ++errors;
        }
        if (kid.hasKey("/Kids")) {
            getAllPagesInternal(kid, visited, seen, media_box, resources);
        } else {
            if (!media_box && !kid.getKey("/MediaBox").isRectangle()) {
                kid.warn(
                    "kid " + std::to_string(i) +
                    " (from 0) MediaBox is undefined; setting to letter / ANSI A");
                kid.replaceKey(
                    "/MediaBox",
                    QPDFObjectHandle::newArray(QPDFObjectHandle::Rectangle(0, 0, 612, 792)));
                ++errors;
            }
            if (!resources) {
                auto res = kid.getKey("/Resources");

                if (!res.isDictionary()) {
                    ++errors;
                    kid.warn(
                        "kid " + std::to_string(i) +
                        " (from 0) Resources is missing or invalid; repairing");
                    kid.replaceKey("/Resources", QPDFObjectHandle::newDictionary());
                }
            }
            auto annots = kid.getKey("/Annots");
            if (!annots.null()) {
                if (!annots.isArray()) {
                    kid.warn(
                        "kid " + std::to_string(i) + " (from 0) Annots is not an array; removing");
                    kid.removeKey("/Annots");
                    ++errors;
                } else {
                    QPDFObjGen::set seen_annots;
                    for (auto& annot: annots.as_array()) {
                        if (!seen_annots.add(annot)) {
                            kid.warn(
                                "kid " + std::to_string(i) +
                                " (from 0) Annots has duplicate entry for annotation " +
                                annot.id_gen().unparse(' '));
                            ++errors;
                        }
                    }
                }
            }

            if (!seen.add(kid)) {
                // Make a copy of the page. This does the same as shallowCopyPage in
                // QPDFPageObjectHelper.
                if (!m->reconstructed_xref) {
                    cur_node.warn(
                        "kid " + std::to_string(i) +
                        " (from 0) appears more than once in the pages tree;"
                        " creating a new page object as a copy");
                    // This needs to be fixed. shallowCopy does not necessarily produce a valid
                    // page.
                    kid = qpdf.makeIndirectObject(QPDFObjectHandle(kid).shallowCopy());
                    seen.add(kid);
                } else {
                    cur_node.warn(
                        "kid " + std::to_string(i) +
                        " (from 0) appears more than once in the pages tree; ignoring duplicate");
                    invalid_page_found = true;
                    kid = QPDFObjectHandle::newNull();
                    continue;
                }
                if (!kid.getKey("/Parent").isSameObjectAs(cur_node)) {
                    // Consider fixing and adding an information message.
                    ++errors;
                }
            }
            if (!kid.isDictionaryOfType("/Page")) {
                kid.warn("/Type key should be /Page but is not; overriding");
                kid.replaceKey("/Type", "/Page"_qpdf);
                ++errors;
            }
            if (m->reconstructed_xref && errors > 2) {
                cur_node.warn(
                    "kid " + std::to_string(i) + " (from 0) has too many errors; ignoring page");
                invalid_page_found = true;
                kid = QPDFObjectHandle::newNull();
                continue;
            }
            all_pages.emplace_back(kid);
        }
    }
}

void
QPDF::updateAllPagesCache()
{
    m->pages.update_cache();
}

void
Pages::update_cache()
{
    // Force regeneration of the pages cache.  We force immediate recalculation of all_pages since
    // users may have references to it that they got from calls to getAllPages().  We can defer
    // recalculation of pageobj_to_pages_pos until needed.
    all_pages.clear();
    pageobj_to_pages_pos.clear();
    pushed_inherited_attributes_to_pages = false;
    cache();
}

void
Pages::flattenPagesTree()
{
    // If not already done, flatten the /Pages structure and initialize pageobj_to_pages_pos.

    if (!pageobj_to_pages_pos.empty()) {
        return;
    }

    // Push inherited objects down to the /Page level.  As a side effect all_pages will also be
    // generated.
    pushInheritedAttributesToPage(true, true);

    QPDFObjectHandle pages = qpdf.getRoot().getKey("/Pages");

    size_t const len = all_pages.size();
    for (size_t pos = 0; pos < len; ++pos) {
        // Populate pageobj_to_pages_pos and fix parent pointer. There should be no duplicates at
        // this point because pushInheritedAttributesToPage calls getAllPages which resolves
        // duplicates.
        insertPageobjToPage(all_pages.at(pos), toI(pos), true);
        all_pages.at(pos).replaceKey("/Parent", pages);
    }

    pages.replaceKey("/Kids", Array(all_pages));
    // /Count has not changed
    if (pages.getKey("/Count").getUIntValue() != len) {
        if (invalid_page_found && pages.getKey("/Count").getUIntValue() > len) {
            pages.replaceKey("/Count", Integer(len));
        } else {
            throw std::runtime_error("/Count is wrong after flattening pages tree");
        }
    }
}

void
QPDF::pushInheritedAttributesToPage()
{
    // Public API should not have access to allow_changes.
    m->pages.pushInheritedAttributesToPage(true, false);
}

void
Pages::pushInheritedAttributesToPage(bool allow_changes, bool warn_skipped_keys)
{
    // Traverse pages tree pushing all inherited resources down to the page level.

    // The record of whether we've done this is cleared by updateAllPagesCache().  If we're warning
    // for skipped keys, re-traverse unconditionally.
    if (pushed_inherited_attributes_to_pages && !warn_skipped_keys) {
        return;
    }

    // Calling cache() resolves any duplicated page objects, repairs broken nodes, and detects
    // loops, so we don't have to do those activities here.
    (void)cache();

    // key_ancestors is a mapping of page attribute keys to a stack of Pages nodes that contain
    // values for them.
    std::map<std::string, std::vector<QPDFObjectHandle>> key_ancestors;
    pushInheritedAttributesToPageInternal(
        m->trailer.getKey("/Root").getKey("/Pages"),
        key_ancestors,
        allow_changes,
        warn_skipped_keys);
    util::assertion(
        key_ancestors.empty(),
        "key_ancestors not empty after pushing inherited attributes to pages");
    pushed_inherited_attributes_to_pages = true;
    ever_pushed_inherited_attributes_to_pages_ = true;
}

void
Pages::pushInheritedAttributesToPageInternal(
    QPDFObjectHandle cur_pages,
    std::map<std::string, std::vector<QPDFObjectHandle>>& key_ancestors,
    bool allow_changes,
    bool warn_skipped_keys)
{
    // Make a list of inheritable keys. Only the keys /MediaBox, /CropBox, /Resources, and /Rotate
    // are inheritable attributes. Push this object onto the stack of pages nodes that have values
    // for this attribute.

    std::set<std::string> inheritable_keys;
    for (auto const& key: cur_pages.getKeys()) {
        if (key == "/MediaBox" || key == "/CropBox" || key == "/Resources" || key == "/Rotate") {
            if (!allow_changes) {
                throw QPDFExc(
                    qpdf_e_internal,
                    m->file->getName(),
                    m->last_object_description,
                    m->file->getLastOffset(),
                    "optimize detected an inheritable attribute when called in no-change mode");
            }

            // This is an inheritable resource
            inheritable_keys.insert(key);
            QPDFObjectHandle oh = cur_pages.getKey(key);
            QTC::TC("qpdf", "QPDF opt direct pages resource", oh.indirect() ? 0 : 1);
            if (!oh.indirect()) {
                if (!oh.isScalar()) {
                    // Replace shared direct object non-scalar resources with indirect objects to
                    // avoid copying large structures around.
                    cur_pages.replaceKey(key, qpdf.makeIndirectObject(oh));
                    oh = cur_pages.getKey(key);
                } else {
                    // It's okay to copy scalars.
                }
            }
            key_ancestors[key].push_back(oh);
            if (key_ancestors[key].size() > 1) {
            }
            // Remove this resource from this node.  It will be reattached at the page level.
            cur_pages.removeKey(key);
        } else if (!(key == "/Type" || key == "/Parent" || key == "/Kids" || key == "/Count")) {
            // Warn when flattening, but not if the key is at the top level (i.e. "/Parent" not
            // set), as we don't change these; but flattening removes intermediate /Pages nodes.
            if (warn_skipped_keys && cur_pages.hasKey("/Parent")) {
                warn(
                    qpdf_e_pages,
                    "Pages object: object " + cur_pages.id_gen().unparse(' '),
                    0,
                    ("Unknown key " + key +
                     " in /Pages object is being discarded as a result of flattening the /Pages "
                     "tree"));
            }
        }
    }

    // Process descendant nodes. This method does not perform loop detection because all code paths
    // that lead here follow a call to getAllPages, which already throws an exception in the event
    // of a loop in the pages tree.
    for (auto& kid: cur_pages.getKey("/Kids").aitems()) {
        if (kid.isDictionaryOfType("/Pages")) {
            pushInheritedAttributesToPageInternal(
                kid, key_ancestors, allow_changes, warn_skipped_keys);
        } else {
            // Add all available inheritable attributes not present in this object to this object.
            for (auto const& iter: key_ancestors) {
                std::string const& key = iter.first;
                if (!kid.hasKey(key)) {
                    kid.replaceKey(key, iter.second.back());
                } else {
                    QTC::TC("qpdf", "QPDF opt page resource hides ancestor");
                }
            }
        }
    }

    // For each inheritable key, pop the stack.  If the stack becomes empty, remove it from the map.
    // That way, the invariant that the list of keys in key_ancestors is exactly those keys for
    // which inheritable attributes are available.

    if (!inheritable_keys.empty()) {
        for (auto const& key: inheritable_keys) {
            key_ancestors[key].pop_back();
            if (key_ancestors[key].empty()) {
                key_ancestors.erase(key);
            }
        }
    } else {
        QTC::TC("qpdf", "QPDF opt no inheritable keys");
    }
}

void
Pages::insertPageobjToPage(QPDFObjectHandle const& obj, int pos, bool check_duplicate)
{
    QPDFObjGen og(obj.getObjGen());
    if (check_duplicate) {
        if (!pageobj_to_pages_pos.insert(std::make_pair(og, pos)).second) {
            // The library never calls insertPageobjToPage in a way that causes this to happen.
            throw QPDFExc(
                qpdf_e_pages,
                m->file->getName(),
                "page " + std::to_string(pos) + " (numbered from zero): object " + og.unparse(' '),
                0,
                "duplicate page reference found; this would cause loss of data");
        }
    } else {
        pageobj_to_pages_pos[og] = pos;
    }
}

void
Pages::insertPage(QPDFObjectHandle newpage, int pos)
{
    // pos is numbered from 0, so pos = 0 inserts at the beginning and pos = npages adds to the end.

    flattenPagesTree();

    if (!newpage.isIndirect()) {
        newpage = qpdf.makeIndirectObject(newpage);
    } else if (newpage.getOwningQPDF() != &qpdf) {
        newpage.getQPDF().pushInheritedAttributesToPage();
        newpage = qpdf.copyForeignObject(newpage);
    } else {
        QTC::TC("qpdf", "QPDF insert indirect page");
    }

    if (pos < 0 || toS(pos) > all_pages.size()) {
        throw std::runtime_error("QPDF::insertPage called with pos out of range");
    }

    QTC::TC(
        "qpdf",
        "QPDF insert page",
        (pos == 0) ? 0 :                         // insert at beginning
            (pos == toI(all_pages.size())) ? 1   // at end
                                           : 2); // insert in middle

    auto og = newpage.getObjGen();
    if (pageobj_to_pages_pos.contains(og)) {
        newpage = qpdf.makeIndirectObject(QPDFObjectHandle(newpage).shallowCopy());
    }

    QPDFObjectHandle pages = qpdf.getRoot().getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    newpage.replaceKey("/Parent", pages);
    kids.insertItem(pos, newpage);
    int npages = static_cast<int>(kids.size());
    pages.replaceKey("/Count", QPDFObjectHandle::newInteger(npages));
    all_pages.insert(all_pages.begin() + pos, newpage);
    for (int i = pos + 1; i < npages; ++i) {
        insertPageobjToPage(all_pages.at(toS(i)), i, false);
    }
    insertPageobjToPage(newpage, pos, true);
}

void
QPDF::removePage(QPDFObjectHandle page)
{
    m->pages.erase(page);
}

void
Pages::erase(QPDFObjectHandle& page)
{
    int pos = qpdf.findPage(page); // also ensures flat /Pages
    QTC::TC(
        "qpdf",
        "QPDF remove page",
        (pos == 0) ? 0 :                             // remove at beginning
            (pos == toI(all_pages.size() - 1)) ? 1   // end
                                               : 2); // remove in middle

    QPDFObjectHandle pages = qpdf.getRoot().getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    kids.eraseItem(pos);
    int npages = static_cast<int>(kids.size());
    pages.replaceKey("/Count", QPDFObjectHandle::newInteger(npages));
    all_pages.erase(all_pages.begin() + pos);
    pageobj_to_pages_pos.erase(page.getObjGen());
    for (int i = pos; i < npages; ++i) {
        m->pages.insertPageobjToPage(all_pages.at(toS(i)), i, false);
    }
}

void
QPDF::addPageAt(QPDFObjectHandle newpage, bool before, QPDFObjectHandle refpage)
{
    int refpos = findPage(refpage);
    if (!before) {
        ++refpos;
    }
    m->pages.insertPage(newpage, refpos);
}

void
QPDF::addPage(QPDFObjectHandle newpage, bool first)
{
    if (first) {
        m->pages.insertPage(newpage, 0);
    } else {
        m->pages.insertPage(
            newpage, getRoot().getKey("/Pages").getKey("/Count").getIntValueAsInt());
    }
}

int
QPDF::findPage(QPDFObjectHandle& page)
{
    return findPage(page.getObjGen());
}

int
QPDF::findPage(QPDFObjGen og)
{
    return m->pages.find(og);
}

int
Pages::find(QPDFObjGen og)
{
    flattenPagesTree();
    auto it = pageobj_to_pages_pos.find(og);
    if (it == pageobj_to_pages_pos.end()) {
        throw QPDFExc(
            qpdf_e_pages,
            m->file->getName(),
            "page object: object " + og.unparse(' '),
            0,
            "page object not referenced in /Pages tree");
    }
    return (*it).second;
}

class QPDFPageDocumentHelper::Members
{
};

QPDFPageDocumentHelper::QPDFPageDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf)
{
}

QPDFPageDocumentHelper&
QPDFPageDocumentHelper::get(QPDF& qpdf)
{
    return qpdf.doc().page_dh();
}

void
QPDFPageDocumentHelper::validate(bool repair)
{
}

std::vector<QPDFPageObjectHelper>
QPDFPageDocumentHelper::getAllPages()
{
    auto& pp = qpdf.doc().pages();
    return {pp.begin(), pp.end()};
}

void
QPDFPageDocumentHelper::pushInheritedAttributesToPage()
{
    qpdf.pushInheritedAttributesToPage();
}

void
QPDFPageDocumentHelper::removeUnreferencedResources()
{
    for (auto& ph: getAllPages()) {
        ph.removeUnreferencedResources();
    }
}

void
QPDFPageDocumentHelper::addPage(QPDFPageObjectHelper newpage, bool first)
{
    qpdf.addPage(newpage.getObjectHandle(), first);
}

void
QPDFPageDocumentHelper::addPageAt(
    QPDFPageObjectHelper newpage, bool before, QPDFPageObjectHelper refpage)
{
    qpdf.addPageAt(newpage.getObjectHandle(), before, refpage.getObjectHandle());
}

void
QPDFPageDocumentHelper::removePage(QPDFPageObjectHelper page)
{
    qpdf.removePage(page.getObjectHandle());
}

void
QPDFPageDocumentHelper::flattenAnnotations(int required_flags, int forbidden_flags)
{
    qpdf.doc().pages().flatten_annotations(required_flags, forbidden_flags);
}

void
Pages::flatten_annotations(int required_flags, int forbidden_flags)
{
    auto& afdh = qpdf.doc().acroform();
    if (afdh.getNeedAppearances()) {
        qpdf.getRoot()
            .getKey("/AcroForm")
            .warn(
                "document does not have updated appearance streams, so form fields "
                "will not be flattened");
    }
    for (QPDFPageObjectHelper ph: all()) {
        QPDFObjectHandle resources = ph.getAttribute("/Resources", true);
        if (!resources.isDictionary()) {
            // As of #1521, this should be impossible unless a user inserted an invalid page.
            resources = ph.getObjectHandle().replaceKeyAndGetNew("/Resources", Dictionary::empty());
        }
        flatten_annotations_for_page(ph, resources, afdh, required_flags, forbidden_flags);
    }
    if (!afdh.getNeedAppearances()) {
        qpdf.getRoot().removeKey("/AcroForm");
    }
}

void
Pages::flatten_annotations_for_page(
    QPDFPageObjectHelper& page,
    QPDFObjectHandle& resources,
    QPDFAcroFormDocumentHelper& afdh,
    int required_flags,
    int forbidden_flags)
{
    bool need_appearances = afdh.getNeedAppearances();
    std::vector<QPDFAnnotationObjectHelper> annots = page.getAnnotations();
    std::vector<QPDFObjectHandle> new_annots;
    std::string new_content;
    int rotate = 0;
    QPDFObjectHandle rotate_obj = page.getObjectHandle().getKey("/Rotate");
    if (rotate_obj.isInteger() && rotate_obj.getIntValue()) {
        rotate = rotate_obj.getIntValueAsInt();
    }
    int next_fx = 1;
    for (auto& aoh: annots) {
        QPDFObjectHandle as = aoh.getAppearanceStream("/N");
        bool is_widget = (aoh.getSubtype() == "/Widget");
        bool process = true;
        if (need_appearances && is_widget) {
            process = false;
        }
        if (process && as.isStream()) {
            if (is_widget) {
                QPDFFormFieldObjectHelper ff = afdh.getFieldForAnnotation(aoh);
                QPDFObjectHandle as_resources = as.getDict().getKey("/Resources");
                if (as_resources.isIndirect()) {
                    ;
                    as.getDict().replaceKey("/Resources", as_resources.shallowCopy());
                    as_resources = as.getDict().getKey("/Resources");
                }
                as_resources.mergeResources(ff.getDefaultResources());
            } else {
                QTC::TC("qpdf", "QPDFPageDocumentHelper non-widget annotation");
            }
            std::string name = resources.getUniqueResourceName("/Fxo", next_fx);
            std::string content =
                aoh.getPageContentForAppearance(name, rotate, required_flags, forbidden_flags);
            if (!content.empty()) {
                resources.mergeResources("<< /XObject << >> >>"_qpdf);
                resources.getKey("/XObject").replaceKey(name, as);
                ++next_fx;
            }
            new_content += content;
        } else if (process && !aoh.getAppearanceDictionary().null()) {
            // If an annotation has no selected appearance stream, just drop the annotation when
            // flattening. This can happen for unchecked checkboxes and radio buttons, popup windows
            // associated with comments that aren't visible, and other types of annotations that
            // aren't visible. Annotations that have no appearance streams at all, such as Link,
            // Popup, and Projection, should be preserved.
        } else {
            new_annots.push_back(aoh.getObjectHandle());
        }
    }
    if (new_annots.size() != annots.size()) {
        QPDFObjectHandle page_oh = page.getObjectHandle();
        if (new_annots.empty()) {
            page_oh.removeKey("/Annots");
        } else {
            QPDFObjectHandle old_annots = page_oh.getKey("/Annots");
            QPDFObjectHandle new_annots_oh = QPDFObjectHandle::newArray(new_annots);
            if (old_annots.isIndirect()) {
                qpdf.replaceObject(old_annots.getObjGen(), new_annots_oh);
            } else {
                page_oh.replaceKey("/Annots", new_annots_oh);
            }
        }
        page.addPageContents(qpdf.newStream("q\n"), true);
        page.addPageContents(qpdf.newStream("\nQ\n" + new_content), false);
    }
}

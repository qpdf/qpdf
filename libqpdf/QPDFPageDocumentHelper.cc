#include <qpdf/QPDFPageDocumentHelper.hh>

#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

QPDFPageDocumentHelper::QPDFPageDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf)
{
}

std::vector<QPDFPageObjectHelper>
QPDFPageDocumentHelper::getAllPages()
{
    std::vector<QPDFPageObjectHelper> pages;
    for (auto const& iter: this->qpdf.getAllPages()) {
        pages.push_back(QPDFPageObjectHelper(iter));
    }
    return pages;
}

void
QPDFPageDocumentHelper::pushInheritedAttributesToPage()
{
    this->qpdf.pushInheritedAttributesToPage();
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
    this->qpdf.addPage(newpage.getObjectHandle(), first);
}

void
QPDFPageDocumentHelper::addPageAt(
    QPDFPageObjectHelper newpage, bool before, QPDFPageObjectHelper refpage)
{
    this->qpdf.addPageAt(newpage.getObjectHandle(), before, refpage.getObjectHandle());
}

void
QPDFPageDocumentHelper::removePage(QPDFPageObjectHelper page)
{
    this->qpdf.removePage(page.getObjectHandle());
}

void
QPDFPageDocumentHelper::flattenAnnotations(int required_flags, int forbidden_flags)
{
    QPDFAcroFormDocumentHelper afdh(this->qpdf);
    if (afdh.getNeedAppearances()) {
        this->qpdf.getRoot()
            .getKey("/AcroForm")
            .warnIfPossible("document does not have updated appearance streams, so form fields "
                            "will not be flattened");
    }
    for (auto& ph: getAllPages()) {
        QPDFObjectHandle resources = ph.getAttribute("/Resources", true);
        if (!resources.isDictionary()) {
            QTC::TC("qpdf", "QPDFPageDocumentHelper flatten resources missing or invalid");
            resources = ph.getObjectHandle().replaceKeyAndGetNew(
                "/Resources", QPDFObjectHandle::newDictionary());
        }
        flattenAnnotationsForPage(ph, resources, afdh, required_flags, forbidden_flags);
    }
    if (!afdh.getNeedAppearances()) {
        this->qpdf.getRoot().removeKey("/AcroForm");
    }
}

void
QPDFPageDocumentHelper::flattenAnnotationsForPage(
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
            QTC::TC("qpdf", "QPDFPageDocumentHelper skip widget need appearances");
            process = false;
        }
        if (process && as.isStream()) {
            if (is_widget) {
                QTC::TC("qpdf", "QPDFPageDocumentHelper merge DR");
                QPDFFormFieldObjectHelper ff = afdh.getFieldForAnnotation(aoh);
                QPDFObjectHandle as_resources = as.getDict().getKey("/Resources");
                if (as_resources.isIndirect()) {
                    QTC::TC("qpdf", "QPDFPageDocumentHelper indirect as resources");
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
        } else if (process) {
            // If an annotation has no appearance stream, just drop the annotation when flattening.
            // This can happen for unchecked checkboxes and radio buttons, popup windows associated
            // with comments that aren't visible, and other types of annotations that aren't
            // visible.
            QTC::TC("qpdf", "QPDFPageDocumentHelper ignore annotation with no appearance");
        } else {
            new_annots.push_back(aoh.getObjectHandle());
        }
    }
    if (new_annots.size() != annots.size()) {
        QPDFObjectHandle page_oh = page.getObjectHandle();
        if (new_annots.empty()) {
            QTC::TC("qpdf", "QPDFPageDocumentHelper remove annots");
            page_oh.removeKey("/Annots");
        } else {
            QPDFObjectHandle old_annots = page_oh.getKey("/Annots");
            QPDFObjectHandle new_annots_oh = QPDFObjectHandle::newArray(new_annots);
            if (old_annots.isIndirect()) {
                QTC::TC("qpdf", "QPDFPageDocumentHelper replace indirect annots");
                this->qpdf.replaceObject(old_annots.getObjGen(), new_annots_oh);
            } else {
                QTC::TC("qpdf", "QPDFPageDocumentHelper replace direct annots");
                page_oh.replaceKey("/Annots", new_annots_oh);
            }
        }
        page.addPageContents(qpdf.newStream("q\n"), true);
        page.addPageContents(qpdf.newStream("\nQ\n" + new_content), false);
    }
}

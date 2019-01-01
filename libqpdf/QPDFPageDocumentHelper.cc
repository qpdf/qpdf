#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QTC.hh>

QPDFPageDocumentHelper::Members::~Members()
{
}

QPDFPageDocumentHelper::Members::Members()
{
}

QPDFPageDocumentHelper::QPDFPageDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf)
{
}

std::vector<QPDFPageObjectHelper>
QPDFPageDocumentHelper::getAllPages()
{
    std::vector<QPDFObjectHandle> const& pages_v = this->qpdf.getAllPages();
    std::vector<QPDFPageObjectHelper> pages;
    for (std::vector<QPDFObjectHandle>::const_iterator iter = pages_v.begin();
         iter != pages_v.end(); ++iter)
    {
        pages.push_back(QPDFPageObjectHelper(*iter));
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
    std::vector<QPDFPageObjectHelper> pages = getAllPages();
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        (*iter).removeUnreferencedResources();
    }
}

void
QPDFPageDocumentHelper::addPage(QPDFPageObjectHelper newpage, bool first)
{
    this->qpdf.addPage(newpage.getObjectHandle(), first);
}

void
QPDFPageDocumentHelper::addPageAt(QPDFPageObjectHelper newpage, bool before,
                                  QPDFPageObjectHelper refpage)
{
    this->qpdf.addPageAt(newpage.getObjectHandle(), before,
                         refpage.getObjectHandle());
}

void
QPDFPageDocumentHelper::removePage(QPDFPageObjectHelper page)
{
    this->qpdf.removePage(page.getObjectHandle());
}


void
QPDFPageDocumentHelper::flattenAnnotations()
{
    QPDFAcroFormDocumentHelper afdh(this->qpdf);
    if (afdh.getNeedAppearances())
    {
        this->qpdf.getRoot().getKey("/AcroForm").warnIfPossible(
            "document does not have updated appearance streams,"
            " so form fields will not be flattened");
    }
    pushInheritedAttributesToPage();
    std::vector<QPDFPageObjectHelper> pages = getAllPages();
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFPageObjectHelper ph(*iter);
        QPDFObjectHandle page_oh = ph.getObjectHandle();
        if (page_oh.getKey("/Resources").isIndirect())
        {
            QTC::TC("qpdf", "QPDFPageDocumentHelper indirect resources");
            page_oh.replaceKey("/Resources",
                               page_oh.getKey("/Resources").shallowCopy());
        }
        QPDFObjectHandle resources = ph.getObjectHandle().getKey("/Resources");
        if (! resources.isDictionary())
        {
            // This should never happen and is not exercised in the
            // test suite
            resources = QPDFObjectHandle::newDictionary();
        }
        flattenAnnotationsForPage(ph, resources, afdh);
    }
    if (! afdh.getNeedAppearances())
    {
        this->qpdf.getRoot().removeKey("/AcroForm");
    }
}

void
QPDFPageDocumentHelper::flattenAnnotationsForPage(
    QPDFPageObjectHelper& page,
    QPDFObjectHandle& resources,
    QPDFAcroFormDocumentHelper& afdh)
{
    bool need_appearances = afdh.getNeedAppearances();
    std::vector<QPDFAnnotationObjectHelper> annots = page.getAnnotations();
    std::vector<QPDFObjectHandle> new_annots;
    std::string new_content;
    int rotate = 0;
    QPDFObjectHandle rotate_obj =
        page.getObjectHandle().getKey("/Rotate");
    if (rotate_obj.isInteger() && rotate_obj.getIntValue())
    {
        rotate = rotate_obj.getIntValue();
    }
    for (std::vector<QPDFAnnotationObjectHelper>::iterator iter =
             annots.begin();
         iter != annots.end(); ++iter)
    {
        QPDFAnnotationObjectHelper& aoh(*iter);
        QPDFObjectHandle as = aoh.getAppearanceStream("/N");
        bool is_widget = (aoh.getSubtype() == "/Widget");
        bool process = true;
        if (need_appearances && is_widget)
        {
            QTC::TC("qpdf", "QPDFPageDocumentHelper skip widget need appearances");
            process = false;
        }
        if (process && (! as.isStream()))
        {
            process = false;
        }
        if (process)
        {
            resources.mergeDictionary(as.getDict().getKey("/Resources"));
            if (is_widget)
            {
                QTC::TC("qpdf", "QPDFPageDocumentHelper merge DR");
                QPDFFormFieldObjectHelper ff = afdh.getFieldForAnnotation(aoh);
                resources.mergeDictionary(ff.getInheritableFieldValue("/DR"));
            }
            else
            {
                QTC::TC("qpdf", "QPDFPageDocumentHelper non-widget annotation");
            }
            new_content += aoh.getPageContentForAppearance(rotate);
        }
        else
        {
            new_annots.push_back(aoh.getObjectHandle());
        }
    }
    if (new_annots.size() != annots.size())
    {
        QPDFObjectHandle page_oh = page.getObjectHandle();
        if (new_annots.empty())
        {
            QTC::TC("qpdf", "QPDFPageDocumentHelper remove annots");
            page_oh.removeKey("/Annots");
        }
        else
        {
            QPDFObjectHandle old_annots = page_oh.getKey("/Annots");
            QPDFObjectHandle new_annots_oh =
                QPDFObjectHandle::newArray(new_annots);
            if (old_annots.isIndirect())
            {
                QTC::TC("qpdf", "QPDFPageDocumentHelper replace indirect annots");
                this->qpdf.replaceObject(
                    old_annots.getObjGen(), new_annots_oh);
            }
            else
            {
                QTC::TC("qpdf", "QPDFPageDocumentHelper replace direct annots");
                page_oh.replaceKey("/Annots", new_annots_oh);
            }
        }
        page.addPageContents(
            QPDFObjectHandle::newStream(&qpdf, "q\n"), true);
        page.addPageContents(
            QPDFObjectHandle::newStream(&qpdf, "\nQ\n" + new_content), false);
    }
}

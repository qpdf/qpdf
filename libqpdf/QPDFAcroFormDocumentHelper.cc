#include <qpdf/QPDFAcroFormDocumentHelper.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

QPDFAcroFormDocumentHelper::Members::~Members()
{
}

QPDFAcroFormDocumentHelper::Members::Members() :
    cache_valid(false)
{
}

QPDFAcroFormDocumentHelper::QPDFAcroFormDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(new Members())
{
}

void
QPDFAcroFormDocumentHelper::invalidateCache()
{
    this->m->cache_valid = false;
    this->m->field_to_annotations.clear();
    this->m->annotation_to_field.clear();
}

bool
QPDFAcroFormDocumentHelper::hasAcroForm()
{
    return this->qpdf.getRoot().hasKey("/AcroForm");
}

std::vector<QPDFFormFieldObjectHelper>
QPDFAcroFormDocumentHelper::getFormFields()
{
    analyze();
    std::vector<QPDFFormFieldObjectHelper> result;
    for (std::map<QPDFObjGen,
             std::vector<QPDFAnnotationObjectHelper> >::iterator iter =
             this->m->field_to_annotations.begin();
         iter != this->m->field_to_annotations.end(); ++iter)
    {
        result.push_back(this->qpdf.getObjectByObjGen((*iter).first));
    }
    return result;
}

std::vector<QPDFAnnotationObjectHelper>
QPDFAcroFormDocumentHelper::getAnnotationsForField(QPDFFormFieldObjectHelper h)
{
    analyze();
    std::vector<QPDFAnnotationObjectHelper> result;
    QPDFObjGen og(h.getObjectHandle().getObjGen());
    if (this->m->field_to_annotations.count(og))
    {
        result = this->m->field_to_annotations[og];
    }
    return result;
}

std::vector<QPDFAnnotationObjectHelper>
QPDFAcroFormDocumentHelper::getWidgetAnnotationsForPage(QPDFPageObjectHelper h)
{
    return h.getAnnotations("/Widget");
}

QPDFFormFieldObjectHelper
QPDFAcroFormDocumentHelper::getFieldForAnnotation(QPDFAnnotationObjectHelper h)
{
    QPDFObjectHandle oh = h.getObjectHandle();
    if (! (oh.isDictionary() &&
           oh.getKey("/Subtype").isName() &&
           (oh.getKey("/Subtype").getName() == "/Widget")))
    {
        throw std::logic_error(
            "QPDFAnnotationObjectHelper::getFieldForAnnotation called for"
            " non-/Widget annotation");
    }
    analyze();
    QPDFFormFieldObjectHelper result(QPDFObjectHandle::newNull());
    QPDFObjGen og(oh.getObjGen());
    if (this->m->annotation_to_field.count(og))
    {
        result = this->m->annotation_to_field[og];
    }
    return result;
}

void
QPDFAcroFormDocumentHelper::analyze()
{
    if (this->m->cache_valid)
    {
        return;
    }
    this->m->cache_valid = true;
    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (! (acroform.isDictionary() && acroform.hasKey("/Fields")))
    {
        return;
    }
    QPDFObjectHandle fields = acroform.getKey("/Fields");
    if (! fields.isArray())
    {
        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper fields not array");
        acroform.warnIfPossible(
            "/Fields key of /AcroForm dictionary is not an array; ignoring");
        fields = QPDFObjectHandle::newArray();
    }

    // Traverse /AcroForm to find annotations and map them
    // bidirectionally to fields.

    std::set<QPDFObjGen> visited;
    size_t nfields = fields.getArrayNItems();
    QPDFObjectHandle null(QPDFObjectHandle::newNull());
    for (size_t i = 0; i < nfields; ++i)
    {
        traverseField(fields.getArrayItem(i), null, 0, visited);
    }

    // All Widget annotations should have been encountered by
    // traversing /AcroForm, but in case any weren't, find them by
    // walking through pages, and treat any widget annotation that is
    // not associated with a field as its own field. This just ensures
    // that requesting the field for any annotation we find through a
    // page's /Annots list will have some associated field. Note that
    // a file that contains this kind of error will probably not
    // actually work with most viewers.

    QPDFPageDocumentHelper dh(this->qpdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
         iter != pages.end(); ++iter)
    {
        QPDFPageObjectHelper ph(*iter);
        std::vector<QPDFAnnotationObjectHelper> annots =
            getWidgetAnnotationsForPage(ph);
        for (std::vector<QPDFAnnotationObjectHelper>::iterator i2 =
                 annots.begin();
             i2 != annots.end(); ++i2)
        {
            QPDFObjectHandle annot((*i2).getObjectHandle());
            QPDFObjGen og(annot.getObjGen());
            if (this->m->annotation_to_field.count(og) == 0)
            {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper orphaned widget");
                // This is not supposed to happen, but it's easy
                // enough for us to handle this case. Treat the
                // annotation as its own field. This could allow qpdf
                // to sensibly handle a case such as a PDF creator
                // adding a self-contained annotation (merged with the
                // field dictionary) to the page's /Annots array and
                // forgetting to also put it in /AcroForm.
                annot.warnIfPossible(
                    "this widget annotation is not"
                    " reachable from /AcroForm in the document catalog");
                this->m->annotation_to_field[og] =
                    QPDFFormFieldObjectHelper(annot);
                this->m->field_to_annotations[og].push_back(
                    QPDFAnnotationObjectHelper(annot));
            }
        }
    }
}

void
QPDFAcroFormDocumentHelper::traverseField(
    QPDFObjectHandle field, QPDFObjectHandle parent, int depth,
    std::set<QPDFObjGen>& visited)
{
    if (depth > 100)
    {
        // Arbitrarily cut off recursion at a fixed depth to avoid
        // specially crafted files that could cause stack overflow.
        return;
    }
    if (! field.isIndirect())
    {
        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper direct field");
        field.warnIfPossible(
            "encountered a direct object as a field or annotation while"
            " traversing /AcroForm; ignoring field or annotation");
        return;
    }
    if (! field.isDictionary())
    {
        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper non-dictionary field");
        field.warnIfPossible(
            "encountered a non-dictionary as a field or annotation while"
            " traversing /AcroForm; ignoring field or annotation");
        return;
    }
    QPDFObjGen og(field.getObjGen());
    if (visited.count(og) != 0)
    {
        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper loop");
        field.warnIfPossible("loop detected while traversing /AcroForm");
        return;
    }
    visited.insert(og);

    // A dictionary encountered while traversing the /AcroForm field
    // may be a form field, an annotation, or the merger of the two. A
    // field that has no fields below it is a terminal. If a terminal
    // field looks like an annotation, it is an annotation because
    // annotation dictionary fields can be merged with terminal field
    // dictionaries. Otherwise, the annotation fields might be there
    // to be inherited by annotations below it.

    bool is_annotation = false;
    bool is_field = (0 == depth);
    QPDFObjectHandle kids = field.getKey("/Kids");
    if (kids.isArray())
    {
        is_field = true;
        size_t nkids = kids.getArrayNItems();
        for (size_t k = 0; k < nkids; ++k)
        {
            traverseField(kids.getArrayItem(k), field, 1 + depth, visited);
        }
    }
    else
    {
        if (field.hasKey("/Parent"))
        {
            is_field = true;
        }
        if (field.hasKey("/Subtype") ||
            field.hasKey("/Rect") ||
            field.hasKey("/AP"))
        {
            is_annotation = true;
        }
    }

    QTC::TC("qpdf", "QPDFAcroFormDocumentHelper field found",
            (depth == 0) ? 0 : 1);
    QTC::TC("qpdf", "QPDFAcroFormDocumentHelper annotation found",
            (is_field ? 0 : 1));

    if (is_annotation)
    {
        QPDFObjectHandle our_field = (is_field ? field : parent);
        this->m->field_to_annotations[our_field.getObjGen()].push_back(
            QPDFAnnotationObjectHelper(field));
        this->m->annotation_to_field[og] =
            QPDFFormFieldObjectHelper(our_field);
    }
}

bool
QPDFAcroFormDocumentHelper::getNeedAppearances()
{
    bool result = false;
    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (acroform.isDictionary() &&
        acroform.getKey("/NeedAppearances").isBool())
    {
        result = acroform.getKey("/NeedAppearances").getBoolValue();
    }
    return result;
}

void
QPDFAcroFormDocumentHelper::setNeedAppearances(bool val)
{
    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (! acroform.isDictionary())
    {
        this->qpdf.getRoot().warnIfPossible(
            "ignoring call to QPDFAcroFormDocumentHelper::setNeedAppearances"
            " on a file that lacks an /AcroForm dictionary");
        return;
    }
    if (val)
    {
        acroform.replaceKey("/NeedAppearances",
                            QPDFObjectHandle::newBool(true));
    }
    else
    {
        acroform.removeKey("/NeedAppearances");
    }
}

void
QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded()
{
    if (! getNeedAppearances())
    {
        return;
    }

    QPDFPageDocumentHelper pdh(this->qpdf);
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    for (std::vector<QPDFPageObjectHelper>::iterator page_iter =
             pages.begin();
         page_iter != pages.end(); ++page_iter)
    {
        std::vector<QPDFAnnotationObjectHelper> annotations =
            getWidgetAnnotationsForPage(*page_iter);
        for (std::vector<QPDFAnnotationObjectHelper>::iterator annot_iter =
                 annotations.begin();
             annot_iter != annotations.end(); ++annot_iter)
        {
            QPDFAnnotationObjectHelper& aoh = *annot_iter;
            QPDFFormFieldObjectHelper ffh =
                getFieldForAnnotation(aoh);
            if (ffh.getFieldType() == "/Btn")
            {
                // Rather than generating appearances for button
                // fields, rely on what's already there. Just make
                // sure /AS is consistent with /V, which we can do by
                // resetting the value of the field back to itself.
                // This code is referenced in a comment in
                // QPDFFormFieldObjectHelper::generateAppearance.
                if (ffh.isRadioButton() || ffh.isCheckbox())
                {
                    ffh.setV(ffh.getValue());
                }
            }
            else
            {
                ffh.generateAppearance(aoh);
            }
        }
    }
    setNeedAppearances(false);
}

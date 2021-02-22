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

void
QPDFAcroFormDocumentHelper::addFormField(QPDFFormFieldObjectHelper ff)
{
    auto acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (! acroform.isDictionary())
    {
        acroform = this->qpdf.makeIndirectObject(
            QPDFObjectHandle::newDictionary());
        this->qpdf.getRoot().replaceKey("/AcroForm", acroform);
    }
    auto fields = acroform.getKey("/Fields");
    if (! fields.isArray())
    {
        fields = QPDFObjectHandle::newArray();
        acroform.replaceKey("/Fields", fields);
    }
    fields.appendItem(ff.getObjectHandle());
    std::set<QPDFObjGen> visited;
    traverseField(
        ff.getObjectHandle(), QPDFObjectHandle::newNull(), 0, visited);
}

void
QPDFAcroFormDocumentHelper::removeFormFields(
    std::set<QPDFObjGen> const& to_remove)
{
    auto acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (! acroform.isDictionary())
    {
        return;
    }
    auto fields = acroform.getKey("/Fields");
    if (! fields.isArray())
    {
        return;
    }

    for (auto const& og: to_remove)
    {
        auto annotations = this->m->field_to_annotations.find(og);
        if (annotations != this->m->field_to_annotations.end())
        {
            for (auto aoh: annotations->second)
            {
                this->m->annotation_to_field.erase(
                    aoh.getObjectHandle().getObjGen());
            }
            this->m->field_to_annotations.erase(og);
        }
    }

    int i = 0;
    while (i < fields.getArrayNItems())
    {
        auto field = fields.getArrayItem(i);
        if (to_remove.count(field.getObjGen()))
        {
            fields.eraseItem(i);
        }
        else
        {
            ++i;
        }
    }
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

std::vector<QPDFFormFieldObjectHelper>
QPDFAcroFormDocumentHelper::getFormFieldsForPage(QPDFPageObjectHelper ph)
{
    std::vector<QPDFFormFieldObjectHelper> result;
    auto widget_annotations = getWidgetAnnotationsForPage(ph);
    for (auto annot: widget_annotations)
    {
        auto field = getFieldForAnnotation(annot);
        field = field.getTopLevelField();
        if (field.getObjectHandle().isDictionary())
        {
            result.push_back(field);
        }
    }
    return result;
}

QPDFFormFieldObjectHelper
QPDFAcroFormDocumentHelper::getFieldForAnnotation(QPDFAnnotationObjectHelper h)
{
    QPDFObjectHandle oh = h.getObjectHandle();
    QPDFFormFieldObjectHelper result(QPDFObjectHandle::newNull());
    if (! (oh.isDictionary() &&
           oh.getKey("/Subtype").isName() &&
           (oh.getKey("/Subtype").getName() == "/Widget")))
    {
        return result;
    }
    analyze();
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
    int nfields = fields.getArrayNItems();
    QPDFObjectHandle null(QPDFObjectHandle::newNull());
    for (int i = 0; i < nfields; ++i)
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
        int nkids = kids.getArrayNItems();
        for (int k = 0; k < nkids; ++k)
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

void
QPDFAcroFormDocumentHelper::transformAnnotations(
    QPDFObjectHandle old_annots,
    std::vector<QPDFObjectHandle>& new_annots,
    std::vector<QPDFObjectHandle>& new_fields,
    std::set<QPDFObjGen>& old_fields,
    QPDFMatrix const& cm,
    QPDF* from_qpdf,
    QPDFAcroFormDocumentHelper* from_afdh)
{
    PointerHolder<QPDFAcroFormDocumentHelper> afdhph;
    if (! from_qpdf)
    {
        // Assume these are from the same QPDF.
        from_qpdf = &this->qpdf;
        from_afdh = this;
    }
    else if ((from_qpdf != &this->qpdf) && (! from_afdh))
    {
        afdhph = new QPDFAcroFormDocumentHelper(*from_qpdf);
        from_afdh = afdhph.getPointer();
    }
    bool foreign = (from_qpdf != &this->qpdf);

    std::set<QPDFObjGen> added_new_fields;

    // This helper prevents us from copying the same object
    // multiple times.
    std::map<QPDFObjGen, QPDFObjectHandle> copied_objects;
    auto maybe_copy_object = [&](QPDFObjectHandle& to_copy) {
        auto og = to_copy.getObjGen();
        if (copied_objects.count(og))
        {
            to_copy = copied_objects[og];
            return false;
        }
        else
        {
            to_copy = this->qpdf.makeIndirectObject(to_copy.shallowCopy());
            copied_objects[og] = to_copy;
            return true;
        }
    };

    for (auto annot: old_annots.aitems())
    {
        if (annot.isStream())
        {
            annot.warnIfPossible("ignoring annotation that's a stream");
            continue;
        }

        // Make copies of annotations and fields down to the
        // appearance streams, preserving all internal referential
        // integrity. When the incoming annotations are from a
        // different file, we first copy them locally. Then, whether
        // local or foreign, we copy them again so that if we bring
        // the same annotation in multiple times (e.g. overlaying a
        // foreign page onto multiple local pages or a local page onto
        // multiple other local pages), we don't create annotations
        // that are referenced in more than one place. If we did that,
        // the effect of applying transformations would be cumulative,
        // which is definitely not what we want. Besides, annotations
        // and fields are not intended to be referenced in multiple
        // places.

        // Determine if this annotation is attached to a form field.
        // If so, the annotation may be the same object as the form
        // field, or the form field may have the annotation as a kid.
        // In either case, we have to walk up the field structure to
        // find the top-level field. Within one iteration through a
        // set of annotations, we don't want to copy the same item
        // more than once. For example, suppose we have field A with
        // kids B, C, and D, each of which has annotations BA, CA, and
        // DA. When we get to BA, we will find that BA is a kid of B
        // which is under A. When we do a copyForeignObject of A, it
        // will also copy everything else because of the indirect
        // references. When we clone BA, we will want to clone A and
        // then update A's clone's kid to point B's clone and B's
        // clone's parent to point to A's clone. The same thing holds
        // for annotatons. Next, when we get to CA, we will again
        // discover that A is the top, but we don't want to re-copy A.
        // We want CA's clone to be linked to the same clone as BA's.
        // Failure to do this will break up things like radio button
        // groups, which all have to kids of the same parent.

        auto ffield = from_afdh->getFieldForAnnotation(annot);
        auto ffield_oh = ffield.getObjectHandle();
        QPDFObjectHandle top_field;
        bool have_field = false;
        bool have_parent = false;
        if (ffield_oh.isStream())
        {
            ffield_oh.warnIfPossible("ignoring form field that's a stream");
        }
        else if ((! ffield_oh.isNull()) && (! ffield_oh.isIndirect()))
        {
            ffield_oh.warnIfPossible("ignoring form field not indirect");
        }
        else if (! ffield_oh.isNull())
        {
            // A field and its associated annotation can be the same
            // object. This matters because we don't want to clone the
            // annotation and field separately in this case.
            have_field = true;
            // Find the top-level field. It may be the field itself.
            top_field = ffield.getTopLevelField(
                &have_parent).getObjectHandle();
            if (foreign)
            {
                // copyForeignObject returns the same value if called
                // multiple times with the same field. Create/retrieve
                // the local copy of the original field. This pulls
                // over everything the field references including
                // annotations and appearance streams, but it's
                // harmless to call copyForeignObject on them too.
                // They will already be copied, so we'll get the right
                // object back.

                top_field = this->qpdf.copyForeignObject(top_field);
                ffield_oh = this->qpdf.copyForeignObject(ffield_oh);
            }
            old_fields.insert(top_field.getObjGen());

            // Traverse the field, copying kids, and preserving
            // integrity.
            std::list<QPDFObjectHandle> queue;
            if (maybe_copy_object(top_field))
            {
                queue.push_back(top_field);
            }
            std::set<QPDFObjGen> seen;
            while (! queue.empty())
            {
                QPDFObjectHandle obj = queue.front();
                queue.pop_front();
                auto orig_og = obj.getObjGen();
                if (seen.count(orig_og))
                {
                    // loop
                    break;
                }
                seen.insert(orig_og);
                auto parent = obj.getKey("/Parent");
                if (parent.isIndirect())
                {
                    auto parent_og = parent.getObjGen();
                    if (copied_objects.count(parent_og))
                    {
                        obj.replaceKey("/Parent", copied_objects[parent_og]);
                    }
                    else
                    {
                        parent.warnIfPossible(
                            "while traversing field " +
                            obj.getObjGen().unparse() +
                            ", found parent (" + parent_og.unparse() +
                            ") that had not been seen, indicating likely"
                            " invalid field structure");
                    }
                }
                auto kids = obj.getKey("/Kids");
                if (kids.isArray())
                {
                    for (int i = 0; i < kids.getArrayNItems(); ++i)
                    {
                        auto kid = kids.getArrayItem(i);
                        if (maybe_copy_object(kid))
                        {
                            kids.setArrayItem(i, kid);
                            queue.push_back(kid);
                        }
                    }
                }
            }

            // Now switch to copies. We already switched for top_field
            maybe_copy_object(ffield_oh);
            ffield = QPDFFormFieldObjectHelper(ffield_oh);
        }

        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper copy annotation",
                (have_field ? 1 : 0) | (foreign ? 2 : 0));
        if (have_field)
        {
            QTC::TC("qpdf", "QPDFAcroFormDocumentHelper field with parent",
                    (have_parent ? 1 : 0) | (foreign ? 2 : 0));
        }
        if (foreign)
        {
            annot = this->qpdf.copyForeignObject(annot);
        }
        maybe_copy_object(annot);

        // Now we have copies, so we can safely mutate.
        if (have_field && ! added_new_fields.count(top_field.getObjGen()))
        {
            new_fields.push_back(top_field);
            added_new_fields.insert(top_field.getObjGen());
        }
        new_annots.push_back(annot);

        // Identify and copy any appearance streams

        auto ah = QPDFAnnotationObjectHelper(annot);
        auto apdict = ah.getAppearanceDictionary();
        std::vector<QPDFObjectHandle> streams;
        auto replace_stream = [](auto& dict, auto& key, auto& old) {
            auto new_stream = old.copyStream();
            dict.replaceKey(key, new_stream);
            return new_stream;
        };
        if (apdict.isDictionary())
        {
            for (auto& ap: apdict.ditems())
            {
                if (ap.second.isStream())
                {
                    streams.push_back(
                        replace_stream(apdict, ap.first, ap.second));
                }
                else if (ap.second.isDictionary())
                {
                    for (auto& ap2: ap.second.ditems())
                    {
                        if (ap2.second.isStream())
                        {
                            streams.push_back(
                                replace_stream(
                                    ap.second, ap2.first, ap2.second));
                        }
                    }
                }
            }
        }

        // Now we can safely mutate the annotation and its appearance
        // streams.
        for (auto& stream: streams)
        {
            auto omatrix = stream.getDict().getKey("/Matrix");
            QPDFMatrix apcm;
            if (omatrix.isArray())
            {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper modify ap matrix");
                auto m1 = omatrix.getArrayAsMatrix();
                apcm = QPDFMatrix(m1);
            }
            apcm.concat(cm);
            auto new_matrix = QPDFObjectHandle::newFromMatrix(apcm);
            stream.getDict().replaceKey("/Matrix", new_matrix);
        }
        auto rect = cm.transformRectangle(
            annot.getKey("/Rect").getArrayAsRectangle());
        annot.replaceKey(
            "/Rect", QPDFObjectHandle::newFromRectangle(rect));
    }
}

void
QPDFAcroFormDocumentHelper::copyFieldsFromForeignPage(
    QPDFPageObjectHelper foreign_page,
    QPDFAcroFormDocumentHelper& foreign_afdh)
{
    std::set<QPDFObjGen> added;
    for (auto field: foreign_afdh.getFormFieldsForPage(foreign_page))
    {
        auto new_field = this->qpdf.copyForeignObject(
            field.getObjectHandle());
        auto og = new_field.getObjGen();
        if (! added.count(og))
        {
            addFormField(new_field);
            added.insert(og);
        }
    }
}

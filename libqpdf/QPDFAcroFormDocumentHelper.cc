#include <qpdf/QPDFAcroFormDocumentHelper.hh>

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/ResourceFinder.hh>

QPDFAcroFormDocumentHelper::Members::Members() :
    cache_valid(false)
{
}

QPDFAcroFormDocumentHelper::QPDFAcroFormDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(new Members())
{
    // We have to analyze up front. Otherwise, when we are adding
    // annotations and fields, we are in a temporarily unstable
    // configuration where some widget annotations are not reachable.
    analyze();
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

QPDFObjectHandle
QPDFAcroFormDocumentHelper::getOrCreateAcroForm()
{
    auto acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        acroform = this->qpdf.getRoot().replaceKeyAndGet(
            "/AcroForm",
            this->qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary()));
    }
    return acroform;
}

void
QPDFAcroFormDocumentHelper::addFormField(QPDFFormFieldObjectHelper ff)
{
    auto acroform = getOrCreateAcroForm();
    auto fields = acroform.getKey("/Fields");
    if (!fields.isArray()) {
        fields =
            acroform.replaceKeyAndGet("/Fields", QPDFObjectHandle::newArray());
    }
    fields.appendItem(ff.getObjectHandle());
    std::set<QPDFObjGen> visited;
    traverseField(
        ff.getObjectHandle(), QPDFObjectHandle::newNull(), 0, visited);
}

void
QPDFAcroFormDocumentHelper::addAndRenameFormFields(
    std::vector<QPDFObjectHandle> fields)
{
    analyze();
    std::map<std::string, std::string> renames;
    std::list<QPDFObjectHandle> queue;
    queue.insert(queue.begin(), fields.begin(), fields.end());
    std::set<QPDFObjGen> seen;
    while (!queue.empty()) {
        QPDFObjectHandle obj = queue.front();
        queue.pop_front();
        auto og = obj.getObjGen();
        if (seen.count(og)) {
            // loop
            continue;
        }
        seen.insert(og);
        auto kids = obj.getKey("/Kids");
        if (kids.isArray()) {
            for (auto kid: kids.aitems()) {
                queue.push_back(kid);
            }
        }

        if (obj.hasKey("/T")) {
            // Find something we can append to the partial name that
            // makes the fully qualified name unique. When we find
            // something, reuse the same suffix for all fields in this
            // group with the same name. We can only change the name
            // of fields that have /T, and this field's /T is always
            // at the end of the fully qualified name, appending to /T
            // has the effect of appending the same thing to the fully
            // qualified name.
            std::string old_name =
                QPDFFormFieldObjectHelper(obj).getFullyQualifiedName();
            if (renames.count(old_name) == 0) {
                std::string new_name = old_name;
                int suffix = 0;
                std::string append;
                while (!getFieldsWithQualifiedName(new_name).empty()) {
                    ++suffix;
                    append = "+" + QUtil::int_to_string(suffix);
                    new_name = old_name + append;
                }
                renames[old_name] = append;
            }
            std::string append = renames[old_name];
            if (!append.empty()) {
                obj.replaceKey(
                    "/T",
                    QPDFObjectHandle::newUnicodeString(
                        obj.getKey("/T").getUTF8Value() + append));
            }
        }
    }

    for (auto i: fields) {
        addFormField(i);
    }
}

void
QPDFAcroFormDocumentHelper::removeFormFields(
    std::set<QPDFObjGen> const& to_remove)
{
    auto acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        return;
    }
    auto fields = acroform.getKey("/Fields");
    if (!fields.isArray()) {
        return;
    }

    for (auto const& og: to_remove) {
        auto annotations = this->m->field_to_annotations.find(og);
        if (annotations != this->m->field_to_annotations.end()) {
            for (auto aoh: annotations->second) {
                this->m->annotation_to_field.erase(
                    aoh.getObjectHandle().getObjGen());
            }
            this->m->field_to_annotations.erase(og);
        }
        auto name = this->m->field_to_name.find(og);
        if (name != this->m->field_to_name.end()) {
            this->m->name_to_fields[name->second].erase(og);
            if (this->m->name_to_fields[name->second].empty()) {
                this->m->name_to_fields.erase(name->second);
            }
            this->m->field_to_name.erase(og);
        }
    }

    int i = 0;
    while (i < fields.getArrayNItems()) {
        auto field = fields.getArrayItem(i);
        if (to_remove.count(field.getObjGen())) {
            fields.eraseItem(i);
        } else {
            ++i;
        }
    }
}

void
QPDFAcroFormDocumentHelper::setFormFieldName(
    QPDFFormFieldObjectHelper ff, std::string const& name)
{
    ff.setFieldAttribute("/T", name);
    std::set<QPDFObjGen> visited;
    auto ff_oh = ff.getObjectHandle();
    traverseField(ff_oh, ff_oh.getKey("/Parent"), 0, visited);
}

std::vector<QPDFFormFieldObjectHelper>
QPDFAcroFormDocumentHelper::getFormFields()
{
    analyze();
    std::vector<QPDFFormFieldObjectHelper> result;
    for (std::map<QPDFObjGen, std::vector<QPDFAnnotationObjectHelper>>::iterator
             iter = this->m->field_to_annotations.begin();
         iter != this->m->field_to_annotations.end();
         ++iter) {
        result.push_back(this->qpdf.getObjectByObjGen((*iter).first));
    }
    return result;
}

std::set<QPDFObjGen>
QPDFAcroFormDocumentHelper::getFieldsWithQualifiedName(std::string const& name)
{
    analyze();
    // Keep from creating an empty entry
    std::set<QPDFObjGen> result;
    auto iter = this->m->name_to_fields.find(name);
    if (iter != this->m->name_to_fields.end()) {
        result = iter->second;
    }
    return result;
}

std::vector<QPDFAnnotationObjectHelper>
QPDFAcroFormDocumentHelper::getAnnotationsForField(QPDFFormFieldObjectHelper h)
{
    analyze();
    std::vector<QPDFAnnotationObjectHelper> result;
    QPDFObjGen og(h.getObjectHandle().getObjGen());
    if (this->m->field_to_annotations.count(og)) {
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
    analyze();
    std::set<QPDFObjGen> added;
    std::vector<QPDFFormFieldObjectHelper> result;
    auto widget_annotations = getWidgetAnnotationsForPage(ph);
    for (auto annot: widget_annotations) {
        auto field = getFieldForAnnotation(annot);
        field = field.getTopLevelField();
        auto og = field.getObjectHandle().getObjGen();
        if (!added.count(og)) {
            added.insert(og);
            if (field.getObjectHandle().isDictionary()) {
                result.push_back(field);
            }
        }
    }
    return result;
}

QPDFFormFieldObjectHelper
QPDFAcroFormDocumentHelper::getFieldForAnnotation(QPDFAnnotationObjectHelper h)
{
    QPDFObjectHandle oh = h.getObjectHandle();
    QPDFFormFieldObjectHelper result(QPDFObjectHandle::newNull());
    if (!oh.isDictionaryOfType("", "/Widget")) {
        return result;
    }
    analyze();
    QPDFObjGen og(oh.getObjGen());
    if (this->m->annotation_to_field.count(og)) {
        result = this->m->annotation_to_field[og];
    }
    return result;
}

void
QPDFAcroFormDocumentHelper::analyze()
{
    if (this->m->cache_valid) {
        return;
    }
    this->m->cache_valid = true;
    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (!(acroform.isDictionary() && acroform.hasKey("/Fields"))) {
        return;
    }
    QPDFObjectHandle fields = acroform.getKey("/Fields");
    if (!fields.isArray()) {
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
    for (int i = 0; i < nfields; ++i) {
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
         iter != pages.end();
         ++iter) {
        QPDFPageObjectHelper ph(*iter);
        std::vector<QPDFAnnotationObjectHelper> annots =
            getWidgetAnnotationsForPage(ph);
        for (std::vector<QPDFAnnotationObjectHelper>::iterator i2 =
                 annots.begin();
             i2 != annots.end();
             ++i2) {
            QPDFObjectHandle annot((*i2).getObjectHandle());
            QPDFObjGen og(annot.getObjGen());
            if (this->m->annotation_to_field.count(og) == 0) {
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
    QPDFObjectHandle field,
    QPDFObjectHandle parent,
    int depth,
    std::set<QPDFObjGen>& visited)
{
    if (depth > 100) {
        // Arbitrarily cut off recursion at a fixed depth to avoid
        // specially crafted files that could cause stack overflow.
        return;
    }
    if (!field.isIndirect()) {
        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper direct field");
        field.warnIfPossible(
            "encountered a direct object as a field or annotation while"
            " traversing /AcroForm; ignoring field or annotation");
        return;
    }
    if (!field.isDictionary()) {
        QTC::TC("qpdf", "QPDFAcroFormDocumentHelper non-dictionary field");
        field.warnIfPossible(
            "encountered a non-dictionary as a field or annotation while"
            " traversing /AcroForm; ignoring field or annotation");
        return;
    }
    QPDFObjGen og(field.getObjGen());
    if (visited.count(og) != 0) {
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
    if (kids.isArray()) {
        is_field = true;
        int nkids = kids.getArrayNItems();
        for (int k = 0; k < nkids; ++k) {
            traverseField(kids.getArrayItem(k), field, 1 + depth, visited);
        }
    } else {
        if (field.hasKey("/Parent")) {
            is_field = true;
        }
        if (field.hasKey("/Subtype") || field.hasKey("/Rect") ||
            field.hasKey("/AP")) {
            is_annotation = true;
        }
    }

    QTC::TC(
        "qpdf", "QPDFAcroFormDocumentHelper field found", (depth == 0) ? 0 : 1);
    QTC::TC(
        "qpdf",
        "QPDFAcroFormDocumentHelper annotation found",
        (is_field ? 0 : 1));

    if (is_annotation) {
        QPDFObjectHandle our_field = (is_field ? field : parent);
        this->m->field_to_annotations[our_field.getObjGen()].push_back(
            QPDFAnnotationObjectHelper(field));
        this->m->annotation_to_field[og] = QPDFFormFieldObjectHelper(our_field);
    }

    if (is_field && (field.hasKey("/T"))) {
        QPDFFormFieldObjectHelper foh(field);
        auto f_og = field.getObjGen();
        std::string name = foh.getFullyQualifiedName();
        auto old = this->m->field_to_name.find(f_og);
        if (old != this->m->field_to_name.end()) {
            // We might be updating after a name change, so remove any
            // old information
            std::string old_name = old->second;
            this->m->name_to_fields[old_name].erase(f_og);
        }
        this->m->field_to_name[f_og] = name;
        this->m->name_to_fields[name].insert(f_og);
    }
}

bool
QPDFAcroFormDocumentHelper::getNeedAppearances()
{
    bool result = false;
    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (acroform.isDictionary() &&
        acroform.getKey("/NeedAppearances").isBool()) {
        result = acroform.getKey("/NeedAppearances").getBoolValue();
    }
    return result;
}

void
QPDFAcroFormDocumentHelper::setNeedAppearances(bool val)
{
    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        this->qpdf.getRoot().warnIfPossible(
            "ignoring call to QPDFAcroFormDocumentHelper::setNeedAppearances"
            " on a file that lacks an /AcroForm dictionary");
        return;
    }
    if (val) {
        acroform.replaceKey(
            "/NeedAppearances", QPDFObjectHandle::newBool(true));
    } else {
        acroform.removeKey("/NeedAppearances");
    }
}

void
QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded()
{
    if (!getNeedAppearances()) {
        return;
    }

    QPDFPageDocumentHelper pdh(this->qpdf);
    std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
    for (std::vector<QPDFPageObjectHelper>::iterator page_iter = pages.begin();
         page_iter != pages.end();
         ++page_iter) {
        std::vector<QPDFAnnotationObjectHelper> annotations =
            getWidgetAnnotationsForPage(*page_iter);
        for (std::vector<QPDFAnnotationObjectHelper>::iterator annot_iter =
                 annotations.begin();
             annot_iter != annotations.end();
             ++annot_iter) {
            QPDFAnnotationObjectHelper& aoh = *annot_iter;
            QPDFFormFieldObjectHelper ffh = getFieldForAnnotation(aoh);
            if (ffh.getFieldType() == "/Btn") {
                // Rather than generating appearances for button
                // fields, rely on what's already there. Just make
                // sure /AS is consistent with /V, which we can do by
                // resetting the value of the field back to itself.
                // This code is referenced in a comment in
                // QPDFFormFieldObjectHelper::generateAppearance.
                if (ffh.isRadioButton() || ffh.isCheckbox()) {
                    ffh.setV(ffh.getValue());
                }
            } else {
                ffh.generateAppearance(aoh);
            }
        }
    }
    setNeedAppearances(false);
}

void
QPDFAcroFormDocumentHelper::adjustInheritedFields(
    QPDFObjectHandle obj,
    bool override_da,
    std::string const& from_default_da,
    bool override_q,
    int from_default_q)
{
    // Override /Q or /DA if needed. If this object has a field type,
    // directly or inherited, it is a field and not just an
    // annotation. In that case, we need to override if we are getting
    // a value from the document that is different from the value we
    // would have gotten from the old document. We must take care not
    // to override an explicit value. It's possible that /FT may be
    // inherited by lower fields that may explicitly set /DA or /Q or
    // that this is a field whose type does not require /DA or /Q and
    // we may be put a value on the field that is unused. This is
    // harmless, so it's not worth trying to work around.

    auto has_explicit = [](QPDFFormFieldObjectHelper& field,
                           std::string const& key) {
        if (field.getObjectHandle().hasKey(key)) {
            return true;
        }
        auto oh = field.getInheritableFieldValue(key);
        if (!oh.isNull()) {
            return true;
        }
        return false;
    };

    if (override_da || override_q) {
        QPDFFormFieldObjectHelper cur_field(obj);
        if (override_da && (!has_explicit(cur_field, "/DA"))) {
            std::string da = cur_field.getDefaultAppearance();
            if (da != from_default_da) {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper override da");
                obj.replaceKey(
                    "/DA", QPDFObjectHandle::newUnicodeString(from_default_da));
            }
        }
        if (override_q && (!has_explicit(cur_field, "/Q"))) {
            int q = cur_field.getQuadding();
            if (q != from_default_q) {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper override q");
                obj.replaceKey(
                    "/Q", QPDFObjectHandle::newInteger(from_default_q));
            }
        }
    }
}

namespace
{
    class ResourceReplacer: public QPDFObjectHandle::TokenFilter
    {
      public:
        ResourceReplacer(
            std::map<std::string, std::map<std::string, std::string>> const&
                dr_map,
            std::map<
                std::string,
                std::map<std::string, std::set<size_t>>> const& rnames);
        virtual ~ResourceReplacer() = default;
        virtual void handleToken(QPDFTokenizer::Token const&) override;

      private:
        size_t offset;
        std::map<std::string, std::map<size_t, std::string>> to_replace;
    };
} // namespace

ResourceReplacer::ResourceReplacer(
    std::map<std::string, std::map<std::string, std::string>> const& dr_map,
    std::map<std::string, std::map<std::string, std::set<size_t>>> const&
        rnames) :
    offset(0)
{
    // We have:
    // * dr_map[resource_type][key] == new_key
    // * rnames[resource_type][key] == set of offsets
    //
    // We want:
    // * to_replace[key][offset] = new_key

    for (auto const& rn_iter: rnames) {
        std::string const& rtype = rn_iter.first;
        auto dr_map_rtype = dr_map.find(rtype);
        if (dr_map_rtype == dr_map.end()) {
            continue;
        }
        auto const& key_offsets = rn_iter.second;
        for (auto const& ko_iter: key_offsets) {
            std::string const& old_key = ko_iter.first;
            auto dr_map_rtype_old = dr_map_rtype->second.find(old_key);
            if (dr_map_rtype_old == dr_map_rtype->second.end()) {
                continue;
            }
            auto const& offsets = ko_iter.second;
            for (auto const& o_iter: offsets) {
                to_replace[old_key][o_iter] = dr_map_rtype_old->second;
            }
        }
    }
}

void
ResourceReplacer::handleToken(QPDFTokenizer::Token const& token)
{
    bool wrote = false;
    if (token.getType() == QPDFTokenizer::tt_name) {
        std::string name =
            QPDFObjectHandle::newName(token.getValue()).getName();
        if (to_replace.count(name) && to_replace[name].count(offset)) {
            QTC::TC("qpdf", "QPDFAcroFormDocumentHelper replaced DA token");
            write(to_replace[name][offset]);
            wrote = true;
        }
    }
    this->offset += token.getRawValue().length();
    if (!wrote) {
        writeToken(token);
    }
}

void
QPDFAcroFormDocumentHelper::adjustDefaultAppearances(
    QPDFObjectHandle obj,
    std::map<std::string, std::map<std::string, std::string>> const& dr_map)
{
    // This method is called on a field that has been copied from
    // another file but whose /DA still refers to resources in the
    // original file's /DR.

    // When appearance streams are generated for variable text fields
    // (see ISO 32000 PDF spec section 12.7.3.3), the field's /DA is
    // used to generate content of the appearance stream. /DA contains
    // references to resources that may be resolved in the document's
    // /DR dictionary, which appears in the document's /AcroForm
    // dictionary. For fields that we copied from other documents, we
    // need to ensure that resources are mapped correctly in the case
    // of conflicting names. For example, if a.pdf's /DR has /F1
    // pointing to one font and b.pdf's /DR also has /F1 but it points
    // elsewhere, we need to make sure appearance streams of fields
    // copied from b.pdf into a.pdf use whatever font /F1 meant in
    // b.pdf, not whatever it means in a.pdf. This method takes care
    // of that. It is only called on fields copied from foreign files.

    // A few notes:
    //
    // * If the from document's /DR and the current document's /DR
    //   have conflicting keys, we have already resolved the conflicts
    //   before calling this method. The dr_map parameter contains the
    //   mapping from old keys to new keys.
    //
    // * /DA may be inherited from the document's /AcroForm
    //   dictionary. By the time this method has been called, we have
    //   already copied any document-level values into the fields to
    //   avoid having them inherit from the new document. This was
    //   done in adjustInheritedFields.

    auto DA = obj.getKey("/DA");
    if (!DA.isString()) {
        return;
    }

    // Find names in /DA. /DA is a string that contains content
    // stream-like code, so we create a stream out of the string and
    // then filter it. We don't attach the stream to anything, so it
    // will get discarded.
    ResourceFinder rf;
    auto da_stream =
        QPDFObjectHandle::newStream(&this->qpdf, DA.getUTF8Value());
    try {
        auto nwarnings = this->qpdf.numWarnings();
        da_stream.parseAsContents(&rf);
        if (this->qpdf.numWarnings() > nwarnings) {
            QTC::TC("qpdf", "QPDFAcroFormDocumentHelper /DA parse error");
        }
    } catch (std::exception& e) {
        // No way to reproduce in test suite right now since error
        // conditions are converted to warnings.
        obj.warnIfPossible(
            std::string("Unable to parse /DA: ") + e.what() +
            "; this form field may not update properly");
        return;
    }

    // Regenerate /DA by filtering its tokens.
    ResourceReplacer rr(dr_map, rf.getNamesByResourceType());
    Pl_Buffer buf_pl("filtered DA");
    da_stream.filterAsContents(&rr, &buf_pl);
    auto buf = buf_pl.getBufferSharedPointer();
    std::string new_da(
        reinterpret_cast<char*>(buf->getBuffer()), buf->getSize());
    obj.replaceKey("/DA", QPDFObjectHandle::newString(new_da));
}

void
QPDFAcroFormDocumentHelper::adjustAppearanceStream(
    QPDFObjectHandle stream,
    std::map<std::string, std::map<std::string, std::string>> dr_map)
{
    // We don't have to modify appearance streams or their resource
    // dictionaries for them to display properly, but we need to do so
    // to make them save to regenerate. Suppose an appearance stream
    // as a font /F1 that is different from /F1 in /DR, and that when
    // we copy the field, /F1 is remapped to /F1_1. When the field is
    // regenerated, /F1_1 won't appear in the stream's resource
    // dictionary, so the regenerated appearance stream will revert to
    // the /F1_1 in /DR. If we adjust existing appearance streams, we
    // are protected from this problem.

    auto dict = stream.getDict();
    auto resources = dict.getKey("/Resources");

    // Make sure this stream has its own private resource dictionary.
    bool was_indirect = resources.isIndirect();
    resources = resources.shallowCopy();
    if (was_indirect) {
        resources = this->qpdf.makeIndirectObject(resources);
    }
    dict.replaceKey("/Resources", resources);
    // Create a dictionary with top-level keys so we can use
    // mergeResources to force them to be unshared. We will also use
    // this to resolve conflicts that may already be in the resource
    // dictionary.
    auto merge_with = QPDFObjectHandle::newDictionary();
    for (auto const& top_key: dr_map) {
        merge_with.replaceKey(top_key.first, QPDFObjectHandle::newDictionary());
    }
    resources.mergeResources(merge_with);
    // Rename any keys in the resource dictionary that we
    // remapped.
    for (auto const& i1: dr_map) {
        std::string const& top_key = i1.first;
        auto subdict = resources.getKey(top_key);
        if (!subdict.isDictionary()) {
            continue;
        }
        for (auto const& i2: i1.second) {
            std::string const& old_key = i2.first;
            std::string const& new_key = i2.second;
            auto existing_new = subdict.getKey(new_key);
            if (!existing_new.isNull()) {
                // The resource dictionary already has a key in it
                // matching what we remapped an old key to, so we'll
                // have to move it out of the way. Stick it in
                // merge_with, which we will re-merge with the
                // dictionary when we're done. We know merge_with
                // already has dictionaries for all the top keys.
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper ap conflict");
                merge_with.getKey(top_key).replaceKey(new_key, existing_new);
            }
            auto existing_old = subdict.getKey(old_key);
            if (!existing_old.isNull()) {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper ap rename");
                subdict.replaceKey(new_key, existing_old).removeKey(old_key);
            }
        }
    }
    // Deal with any any conflicts by re-merging with merge_with and
    // updating our local copy of dr_map, which we will use to modify
    // the stream contents.
    resources.mergeResources(merge_with, &dr_map);
    // Remove empty subdictionaries
    for (auto iter: resources.ditems()) {
        if (iter.second.isDictionary() && (iter.second.getKeys().size() == 0)) {
            resources.removeKey(iter.first);
        }
    }

    // Now attach a token filter to replace the actual resources.
    ResourceFinder rf;
    try {
        auto nwarnings = this->qpdf.numWarnings();
        stream.parseAsContents(&rf);
        if (this->qpdf.numWarnings() > nwarnings) {
            QTC::TC("qpdf", "QPDFAcroFormDocumentHelper AP parse error");
        }
        auto rr = new ResourceReplacer(dr_map, rf.getNamesByResourceType());
        auto tf = std::shared_ptr<QPDFObjectHandle::TokenFilter>(rr);
        stream.addTokenFilter(tf);
    } catch (std::exception& e) {
        // No way to reproduce in test suite right now since error
        // conditions are converted to warnings.
        stream.warnIfPossible(
            std::string("Unable to parse appearance stream: ") + e.what());
    }
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
    std::shared_ptr<QPDFAcroFormDocumentHelper> afdhph;
    if (!from_qpdf) {
        // Assume these are from the same QPDF.
        from_qpdf = &this->qpdf;
        from_afdh = this;
    } else if ((from_qpdf != &this->qpdf) && (!from_afdh)) {
        afdhph = std::make_shared<QPDFAcroFormDocumentHelper>(*from_qpdf);
        from_afdh = afdhph.get();
    }
    bool foreign = (from_qpdf != &this->qpdf);

    // It's possible that we will transform annotations that don't
    // include any form fields. This code takes care not to muck
    // around with /AcroForm unless we have to.

    QPDFObjectHandle acroform = this->qpdf.getRoot().getKey("/AcroForm");
    QPDFObjectHandle from_acroform = from_qpdf->getRoot().getKey("/AcroForm");

    // /DA and /Q may be inherited from the document-level /AcroForm
    // dictionary. If we are copying a foreign stream and the stream
    // is getting one of these values from its document's /AcroForm,
    // we will need to copy the value explicitly so that it doesn't
    // start getting its default from the destination document.
    bool override_da = false;
    bool override_q = false;
    std::string from_default_da;
    int from_default_q = 0;
    // If we copy any form fields, we will need to merge the source
    // document's /DR into this document's /DR.
    QPDFObjectHandle from_dr = QPDFObjectHandle::newNull();
    if (foreign) {
        std::string default_da;
        int default_q = 0;
        if (acroform.isDictionary()) {
            if (acroform.getKey("/DA").isString()) {
                default_da = acroform.getKey("/DA").getUTF8Value();
            }
            if (acroform.getKey("/Q").isInteger()) {
                default_q = acroform.getKey("/Q").getIntValueAsInt();
            }
        }
        if (from_acroform.isDictionary()) {
            if (from_acroform.getKey("/DR").isDictionary()) {
                from_dr = from_acroform.getKey("/DR");
                if (!from_dr.isIndirect()) {
                    from_dr = from_qpdf->makeIndirectObject(from_dr);
                }
                from_dr = this->qpdf.copyForeignObject(from_dr);
            }
            if (from_acroform.getKey("/DA").isString()) {
                from_default_da = from_acroform.getKey("/DA").getUTF8Value();
            }
            if (from_acroform.getKey("/Q").isInteger()) {
                from_default_q = from_acroform.getKey("/Q").getIntValueAsInt();
            }
        }
        if (from_default_da != default_da) {
            override_da = true;
        }
        if (from_default_q != default_q) {
            override_q = true;
        }
    }

    // If we have to merge /DR, we will need a mapping of conflicting
    // keys for rewriting /DA. Set this up for lazy initialization in
    // case we encounter any form fields.
    std::map<std::string, std::map<std::string, std::string>> dr_map;
    bool initialized_dr_map = false;
    QPDFObjectHandle dr = QPDFObjectHandle::newNull();
    auto init_dr_map = [&]() {
        if (!initialized_dr_map) {
            initialized_dr_map = true;
            // Ensure that we have a /DR that is an indirect
            // dictionary object.
            if (!acroform.isDictionary()) {
                acroform = getOrCreateAcroForm();
            }
            dr = acroform.getKey("/DR");
            if (!dr.isDictionary()) {
                dr = QPDFObjectHandle::newDictionary();
            }
            dr.makeResourcesIndirect(this->qpdf);
            if (!dr.isIndirect()) {
                dr = acroform.replaceKeyAndGet(
                    "/DR", this->qpdf.makeIndirectObject(dr));
            }
            // Merge the other document's /DR, creating a conflict
            // map. mergeResources checks to make sure both objects
            // are dictionaries. By this point, if this is foreign,
            // from_dr has been copied, so we use the target qpdf as
            // the owning qpdf.
            from_dr.makeResourcesIndirect(this->qpdf);
            dr.mergeResources(from_dr, &dr_map);

            if (from_afdh->getNeedAppearances()) {
                setNeedAppearances(true);
            }
        }
    };

    // This helper prevents us from copying the same object
    // multiple times.
    std::map<QPDFObjGen, QPDFObjectHandle> orig_to_copy;
    auto maybe_copy_object = [&](QPDFObjectHandle& to_copy) {
        auto og = to_copy.getObjGen();
        if (orig_to_copy.count(og)) {
            to_copy = orig_to_copy[og];
            return false;
        } else {
            to_copy = this->qpdf.makeIndirectObject(to_copy.shallowCopy());
            orig_to_copy[og] = to_copy;
            return true;
        }
    };

    // Now do the actual copies.

    std::set<QPDFObjGen> added_new_fields;
    for (auto annot: old_annots.aitems()) {
        if (annot.isStream()) {
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
        // for annotations. Next, when we get to CA, we will again
        // discover that A is the top, but we don't want to re-copy A.
        // We want CA's clone to be linked to the same clone as BA's.
        // Failure to do this will break up things like radio button
        // groups, which all have to kids of the same parent.

        auto ffield = from_afdh->getFieldForAnnotation(annot);
        auto ffield_oh = ffield.getObjectHandle();
        QPDFObjectHandle top_field;
        bool have_field = false;
        bool have_parent = false;
        if (ffield_oh.isStream()) {
            ffield_oh.warnIfPossible("ignoring form field that's a stream");
        } else if ((!ffield_oh.isNull()) && (!ffield_oh.isIndirect())) {
            ffield_oh.warnIfPossible("ignoring form field not indirect");
        } else if (!ffield_oh.isNull()) {
            // A field and its associated annotation can be the same
            // object. This matters because we don't want to clone the
            // annotation and field separately in this case.
            have_field = true;
            // Find the top-level field. It may be the field itself.
            top_field = ffield.getTopLevelField(&have_parent).getObjectHandle();
            if (foreign) {
                // copyForeignObject returns the same value if called
                // multiple times with the same field. Create/retrieve
                // the local copy of the original field. This pulls
                // over everything the field references including
                // annotations and appearance streams, but it's
                // harmless to call copyForeignObject on them too.
                // They will already be copied, so we'll get the right
                // object back.

                // top_field and ffield_oh are known to be indirect.
                top_field = this->qpdf.copyForeignObject(top_field);
                ffield_oh = this->qpdf.copyForeignObject(ffield_oh);
            } else {
                // We don't need to add top_field to old_fields if
                // it's foreign because the new copy of the foreign
                // field won't be referenced anywhere. It's just the
                // starting point for us to make an additional local
                // copy of.
                old_fields.insert(top_field.getObjGen());
            }

            // Traverse the field, copying kids, and preserving
            // integrity.
            std::list<QPDFObjectHandle> queue;
            if (maybe_copy_object(top_field)) {
                queue.push_back(top_field);
            }
            std::set<QPDFObjGen> seen;
            while (!queue.empty()) {
                QPDFObjectHandle obj = queue.front();
                queue.pop_front();
                auto orig_og = obj.getObjGen();
                if (seen.count(orig_og)) {
                    // loop
                    break;
                }
                seen.insert(orig_og);
                auto parent = obj.getKey("/Parent");
                if (parent.isIndirect()) {
                    auto parent_og = parent.getObjGen();
                    if (orig_to_copy.count(parent_og)) {
                        obj.replaceKey("/Parent", orig_to_copy[parent_og]);
                    } else {
                        parent.warnIfPossible(
                            "while traversing field " +
                            obj.getObjGen().unparse() + ", found parent (" +
                            parent_og.unparse() +
                            ") that had not been seen, indicating likely"
                            " invalid field structure");
                    }
                }
                auto kids = obj.getKey("/Kids");
                if (kids.isArray()) {
                    for (int i = 0; i < kids.getArrayNItems(); ++i) {
                        auto kid = kids.getArrayItem(i);
                        if (maybe_copy_object(kid)) {
                            kids.setArrayItem(i, kid);
                            queue.push_back(kid);
                        }
                    }
                }

                if (override_da || override_q) {
                    adjustInheritedFields(
                        obj,
                        override_da,
                        from_default_da,
                        override_q,
                        from_default_q);
                }
                if (foreign) {
                    // Lazily initialize our /DR and the conflict map.
                    init_dr_map();
                    // The spec doesn't say anything about /DR on the
                    // field, but lots of writers put one there, and
                    // it is frequently the same as the document-level
                    // /DR. To avoid having the field's /DR point to
                    // information that we are not maintaining, just
                    // reset it to that if it exists. Empirical
                    // evidence suggests that many readers, including
                    // Acrobat, Adobe Acrobat Reader, chrome, firefox,
                    // the mac Preview application, and several of the
                    // free readers on Linux all ignore /DR at the
                    // field level.
                    if (obj.hasKey("/DR")) {
                        obj.replaceKey("/DR", dr);
                    }
                }
                if (foreign && obj.getKey("/DA").isString() &&
                    (!dr_map.empty())) {
                    adjustDefaultAppearances(obj, dr_map);
                }
            }

            // Now switch to copies. We already switched for top_field
            maybe_copy_object(ffield_oh);
            ffield = QPDFFormFieldObjectHelper(ffield_oh);
        }

        QTC::TC(
            "qpdf",
            "QPDFAcroFormDocumentHelper copy annotation",
            (have_field ? 1 : 0) | (foreign ? 2 : 0));
        if (have_field) {
            QTC::TC(
                "qpdf",
                "QPDFAcroFormDocumentHelper field with parent",
                (have_parent ? 1 : 0) | (foreign ? 2 : 0));
        }
        if (foreign) {
            if (!annot.isIndirect()) {
                annot = from_qpdf->makeIndirectObject(annot);
            }
            annot = this->qpdf.copyForeignObject(annot);
        }
        maybe_copy_object(annot);

        // Now we have copies, so we can safely mutate.
        if (have_field && !added_new_fields.count(top_field.getObjGen())) {
            new_fields.push_back(top_field);
            added_new_fields.insert(top_field.getObjGen());
        }
        new_annots.push_back(annot);

        // Identify and copy any appearance streams

        auto ah = QPDFAnnotationObjectHelper(annot);
        auto apdict = ah.getAppearanceDictionary();
        std::vector<QPDFObjectHandle> streams;
        auto replace_stream = [](auto& dict, auto& key, auto& old) {
            return dict.replaceKeyAndGet(key, old.copyStream());
        };
        if (apdict.isDictionary()) {
            for (auto& ap: apdict.ditems()) {
                if (ap.second.isStream()) {
                    streams.push_back(
                        replace_stream(apdict, ap.first, ap.second));
                } else if (ap.second.isDictionary()) {
                    for (auto& ap2: ap.second.ditems()) {
                        if (ap2.second.isStream()) {
                            streams.push_back(
                                // line-break
                                replace_stream(
                                    ap.second, ap2.first, ap2.second));
                        }
                    }
                }
            }
        }

        // Now we can safely mutate the annotation and its appearance
        // streams.
        for (auto& stream: streams) {
            auto dict = stream.getDict();
            auto omatrix = dict.getKey("/Matrix");
            QPDFMatrix apcm;
            if (omatrix.isArray()) {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper modify ap matrix");
                auto m1 = omatrix.getArrayAsMatrix();
                apcm = QPDFMatrix(m1);
            }
            apcm.concat(cm);
            auto new_matrix = QPDFObjectHandle::newFromMatrix(apcm);
            if (omatrix.isArray() || (apcm != QPDFMatrix())) {
                dict.replaceKey("/Matrix", new_matrix);
            }
            auto resources = dict.getKey("/Resources");
            if ((!dr_map.empty()) && resources.isDictionary()) {
                adjustAppearanceStream(stream, dr_map);
            }
        }
        auto rect =
            cm.transformRectangle(annot.getKey("/Rect").getArrayAsRectangle());
        annot.replaceKey("/Rect", QPDFObjectHandle::newFromRectangle(rect));
    }
}

void
QPDFAcroFormDocumentHelper::fixCopiedAnnotations(
    QPDFObjectHandle to_page,
    QPDFObjectHandle from_page,
    QPDFAcroFormDocumentHelper& from_afdh,
    std::set<QPDFObjGen>* added_fields)
{
    auto old_annots = from_page.getKey("/Annots");
    if ((!old_annots.isArray()) || (old_annots.getArrayNItems() == 0)) {
        return;
    }

    std::vector<QPDFObjectHandle> new_annots;
    std::vector<QPDFObjectHandle> new_fields;
    std::set<QPDFObjGen> old_fields;
    transformAnnotations(
        old_annots,
        new_annots,
        new_fields,
        old_fields,
        QPDFMatrix(),
        &(from_afdh.getQPDF()),
        &from_afdh);

    to_page.replaceKey("/Annots", QPDFObjectHandle::newArray(new_annots));
    addAndRenameFormFields(new_fields);
    if (added_fields) {
        for (auto f: new_fields) {
            added_fields->insert(f.getObjGen());
        }
    }
}

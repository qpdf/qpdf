#include <qpdf/QPDFAcroFormDocumentHelper.hh>

#include <qpdf/AcroForm.hh>

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/ResourceFinder.hh>
#include <qpdf/Util.hh>

#include <deque>
#include <utility>

using namespace qpdf;
using namespace qpdf::impl;
using namespace std::literals;

class QPDFAcroFormDocumentHelper::Members: public AcroForm
{
  public:
    Members(QPDF& qpdf) :
        AcroForm(qpdf.doc())
    {
    }
};

QPDFAcroFormDocumentHelper::QPDFAcroFormDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(std::make_shared<Members>(qpdf))
{
}

QPDFAcroFormDocumentHelper&
QPDFAcroFormDocumentHelper::get(QPDF& qpdf)
{
    return qpdf.doc().acroform();
}

void
QPDFAcroFormDocumentHelper::validate(bool repair)
{
    m->validate(repair);
}

void
AcroForm::validate(bool repair)
{
    invalidateCache();
    analyze();
}

void
QPDFAcroFormDocumentHelper::invalidateCache()
{
    m->invalidateCache();
}

void
AcroForm::invalidateCache()
{
    cache_valid_ = false;
    fields_.clear();
    annotation_to_field_.clear();
    bad_fields_.clear();
    name_to_fields_.clear();
}

bool
QPDFAcroFormDocumentHelper::hasAcroForm()
{
    return m->hasAcroForm();
}

bool
AcroForm::hasAcroForm()
{
    return qpdf.getRoot().hasKey("/AcroForm");
}

QPDFObjectHandle
AcroForm::getOrCreateAcroForm()
{
    auto acroform = qpdf.getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        acroform = qpdf.getRoot().replaceKeyAndGetNew(
            "/AcroForm", qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary()));
    }
    return acroform;
}

void
QPDFAcroFormDocumentHelper::addFormField(QPDFFormFieldObjectHelper ff)
{
    m->addFormField(ff);
}

void
AcroForm::addFormField(QPDFFormFieldObjectHelper ff)
{
    auto acroform = getOrCreateAcroForm();
    auto fields = acroform.getKey("/Fields");
    if (!fields.isArray()) {
        fields = acroform.replaceKeyAndGetNew("/Fields", QPDFObjectHandle::newArray());
    }
    fields.appendItem(ff.getObjectHandle());
    traverseField(ff.getObjectHandle(), {}, 0);
}

void
QPDFAcroFormDocumentHelper::addAndRenameFormFields(std::vector<QPDFObjectHandle> fields)
{
    m->addAndRenameFormFields(fields);
}

void
AcroForm::addAndRenameFormFields(std::vector<QPDFObjectHandle> fields)
{
    analyze();
    std::map<std::string, std::string> renames;
    QPDFObjGen::set seen;
    for (std::list<QPDFObjectHandle> queue{fields.begin(), fields.end()}; !queue.empty();
         queue.pop_front()) {
        auto& obj = queue.front();
        if (seen.add(obj)) {
            auto kids = obj.getKey("/Kids");
            if (kids.isArray()) {
                for (auto const& kid: kids.aitems()) {
                    queue.push_back(kid);
                }
            }

            if (obj.hasKey("/T")) {
                // Find something we can append to the partial name that makes the fully qualified
                // name unique. When we find something, reuse the same suffix for all fields in this
                // group with the same name. We can only change the name of fields that have /T, and
                // this field's /T is always at the end of the fully qualified name, appending to /T
                // has the effect of appending the same thing to the fully qualified name.
                std::string old_name = QPDFFormFieldObjectHelper(obj).getFullyQualifiedName();
                if (!renames.contains(old_name)) {
                    std::string new_name = old_name;
                    int suffix = 0;
                    std::string append;
                    while (!getFieldsWithQualifiedName(new_name).empty()) {
                        ++suffix;
                        append = "+" + std::to_string(suffix);
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
    }

    for (auto const& i: fields) {
        addFormField(i);
    }
}

void
QPDFAcroFormDocumentHelper::removeFormFields(std::set<QPDFObjGen> const& to_remove)
{
    m->removeFormFields(to_remove);
}

void
AcroForm::removeFormFields(std::set<QPDFObjGen> const& to_remove)
{
    auto acroform = qpdf.getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        return;
    }
    auto fields = acroform.getKey("/Fields");
    if (!fields.isArray()) {
        return;
    }

    for (auto const& og: to_remove) {
        auto it = fields_.find(og);
        if (it != fields_.end()) {
            for (auto aoh: it->second.annotations) {
                annotation_to_field_.erase(aoh.getObjectHandle().getObjGen());
            }
            auto const& name = it->second.name;
            if (!name.empty()) {
                name_to_fields_[name].erase(og);
                if (name_to_fields_[name].empty()) {
                    name_to_fields_.erase(name);
                }
            }
            fields_.erase(og);
        }
    }

    int i = 0;
    while (std::cmp_less(i, fields.size())) {
        auto field = fields.getArrayItem(i);
        if (to_remove.contains(field.getObjGen())) {
            fields.eraseItem(i);
        } else {
            ++i;
        }
    }
}

void
QPDFAcroFormDocumentHelper::setFormFieldName(QPDFFormFieldObjectHelper ff, std::string const& name)
{
    m->setFormFieldName(ff, name);
}

void
AcroForm::setFormFieldName(QPDFFormFieldObjectHelper ff, std::string const& name)
{
    ff.setFieldAttribute("/T", name);
    traverseField(ff, ff["/Parent"], 0);
}

std::vector<QPDFFormFieldObjectHelper>
QPDFAcroFormDocumentHelper::getFormFields()
{
    return m->getFormFields();
}

std::vector<QPDFFormFieldObjectHelper>
AcroForm::getFormFields()
{
    analyze();
    std::vector<QPDFFormFieldObjectHelper> result;
    for (auto const& [og, data]: fields_) {
        if (!data.annotations.empty()) {
            result.emplace_back(qpdf.getObject(og));
        }
    }
    return result;
}

std::set<QPDFObjGen>
QPDFAcroFormDocumentHelper::getFieldsWithQualifiedName(std::string const& name)
{
    return m->getFieldsWithQualifiedName(name);
}

std::set<QPDFObjGen>
AcroForm::getFieldsWithQualifiedName(std::string const& name)
{
    analyze();
    // Keep from creating an empty entry
    auto iter = name_to_fields_.find(name);
    if (iter != name_to_fields_.end()) {
        return iter->second;
    }
    return {};
}

std::vector<QPDFAnnotationObjectHelper>
QPDFAcroFormDocumentHelper::getAnnotationsForField(QPDFFormFieldObjectHelper h)
{
    return m->getAnnotationsForField(h);
}

std::vector<QPDFAnnotationObjectHelper>
AcroForm::getAnnotationsForField(QPDFFormFieldObjectHelper h)
{
    analyze();
    std::vector<QPDFAnnotationObjectHelper> result;
    QPDFObjGen og(h.getObjectHandle().getObjGen());
    if (fields_.contains(og)) {
        result = fields_[og].annotations;
    }
    return result;
}

std::vector<QPDFAnnotationObjectHelper>
QPDFAcroFormDocumentHelper::getWidgetAnnotationsForPage(QPDFPageObjectHelper h)
{
    return m->getWidgetAnnotationsForPage(h);
}

std::vector<QPDFAnnotationObjectHelper>
AcroForm::getWidgetAnnotationsForPage(QPDFPageObjectHelper h)
{
    return h.getAnnotations("/Widget");
}

std::vector<QPDFFormFieldObjectHelper>
QPDFAcroFormDocumentHelper::getFormFieldsForPage(QPDFPageObjectHelper ph)
{
    return m->getFormFieldsForPage(ph);
}

std::vector<QPDFFormFieldObjectHelper>
AcroForm::getFormFieldsForPage(QPDFPageObjectHelper ph)
{
    analyze();
    QPDFObjGen::set todo;
    std::vector<QPDFFormFieldObjectHelper> result;
    for (auto& annot: getWidgetAnnotationsForPage(ph)) {
        auto field = getFieldForAnnotation(annot).getTopLevelField();
        if (todo.add(field) && field.getObjectHandle().isDictionary()) {
            result.push_back(field);
        }
    }
    return result;
}

QPDFFormFieldObjectHelper
QPDFAcroFormDocumentHelper::getFieldForAnnotation(QPDFAnnotationObjectHelper h)
{
    return m->getFieldForAnnotation(h);
}

QPDFFormFieldObjectHelper
AcroForm::getFieldForAnnotation(QPDFAnnotationObjectHelper h)
{
    QPDFObjectHandle oh = h.getObjectHandle();
    if (!oh.isDictionaryOfType("", "/Widget")) {
        return Null::temp();
    }
    analyze();
    QPDFObjGen og(oh.getObjGen());
    if (annotation_to_field_.contains(og)) {
        return annotation_to_field_[og];
    }
    return Null::temp();
}

void
AcroForm::analyze()
{
    if (cache_valid_) {
        return;
    }
    cache_valid_ = true;
    QPDFObjectHandle acroform = qpdf.getRoot().getKey("/AcroForm");
    if (!(acroform.isDictionary() && acroform.hasKey("/Fields"))) {
        return;
    }
    QPDFObjectHandle fields = acroform.getKey("/Fields");
    if (Array fa = fields) {
        // Traverse /AcroForm to find annotations and map them bidirectionally to fields.
        for (auto const& field: fa) {
            traverseField(field, {}, 0);
        }
    } else {
        acroform.warn("/Fields key of /AcroForm dictionary is not an array; ignoring");
        fields = Array::empty();
    }

    // All Widget annotations should have been encountered by traversing /AcroForm, but in case any
    // weren't, find them by walking through pages, and treat any widget annotation that is not
    // associated with a field as its own field. This just ensures that requesting the field for any
    // annotation we find through a page's /Annots list will have some associated field. Note that
    // a file that contains this kind of error will probably not
    // actually work with most viewers.

    for (QPDFPageObjectHelper ph: pages) {
        for (auto const& iter: getWidgetAnnotationsForPage(ph)) {
            QPDFObjectHandle annot(iter.getObjectHandle());
            QPDFObjGen og(annot.getObjGen());
            if (!annotation_to_field_.contains(og)) {
                // This is not supposed to happen, but it's easy enough for us to handle this case.
                // Treat the annotation as its own field. This could allow qpdf to sensibly handle a
                // case such as a PDF creator adding a self-contained annotation (merged with the
                // field dictionary) to the page's /Annots array and forgetting to also put it in
                // /AcroForm.
                annot.warn(
                    "this widget annotation is not reachable from /AcroForm in the document "
                    "catalog");
                annotation_to_field_[og] = QPDFFormFieldObjectHelper(annot);
                fields_[og].annotations.emplace_back(annot);
            }
        }
    }
}

bool
AcroForm::traverseField(QPDFObjectHandle field, QPDFObjectHandle const& parent, int depth)
{
    if (depth > 100) {
        // Arbitrarily cut off recursion at a fixed depth to avoid specially crafted files that
        // could cause stack overflow.
        return false;
    }
    if (!field.indirect()) {
        field.warn(
            "encountered a direct object as a field or annotation while traversing /AcroForm; "
            "ignoring field or annotation");
        return false;
    }
    if (field == parent) {
        field.warn("loop detected while traversing /AcroForm");
        return false;
    }
    if (!field.isDictionary()) {
        field.warn(
            "encountered a non-dictionary as a field or annotation while traversing /AcroForm; "
            "ignoring field or annotation");
        return false;
    }
    QPDFObjGen og(field.getObjGen());
    if (fields_.contains(og) || annotation_to_field_.contains(og) || bad_fields_.contains(og)) {
        field.warn("loop detected while traversing /AcroForm");
        return false;
    }

    // A dictionary encountered while traversing the /AcroForm field may be a form field, an
    // annotation, or the merger of the two. A field that has no fields below it is a terminal. If a
    // terminal field looks like an annotation, it is an annotation because annotation dictionary
    // fields can be merged with terminal field dictionaries. Otherwise, the annotation fields might
    // be there to be inherited by annotations below it.

    Array Kids = field["/Kids"];
    const bool is_field = depth == 0 || Kids || field.hasKey("/Parent");
    const bool is_annotation =
        !Kids && (field.hasKey("/Subtype") || field.hasKey("/Rect") || field.hasKey("/AP"));

    QTC::TC("qpdf", "QPDFAcroFormDocumentHelper field found", (depth == 0) ? 0 : 1);
    QTC::TC("qpdf", "QPDFAcroFormDocumentHelper annotation found", (is_field ? 0 : 1));

    if (!is_field && !is_annotation) {
        field.warn(
            "encountered an object that is neither field nor annotation while traversing "
            "/AcroForm");
        return false;
    }

    if (is_annotation) {
        QPDFObjectHandle our_field = (is_field ? field : parent);
        fields_[our_field.getObjGen()].annotations.emplace_back(field);
        annotation_to_field_[og] = QPDFFormFieldObjectHelper(our_field);
    }

    if (is_field && depth != 0 && field["/Parent"] != parent) {
        for (auto const& kid: Array(field["/Parent"]["/Kids"])) {
            if (kid == field) {
                field.warn("while traversing /AcroForm found field with two parents");
                return true;
            }
        }
        for (auto const& kid: Array(field["/Parent"]["/Kids"])) {
            if (kid == parent) {
                field.warn("loop detected while traversing /AcroForm");
                return false;
            }
        }
        field.warn("encountered invalid /Parent entry while traversing /AcroForm; correcting");
        field.replaceKey("/Parent", parent);
    }

    if (is_field && field.hasKey("/T")) {
        QPDFFormFieldObjectHelper foh(field);
        std::string name = foh.getFullyQualifiedName();
        auto old = fields_.find(og);
        if (old != fields_.end() && !old->second.name.empty()) {
            // We might be updating after a name change, so remove any old information
            name_to_fields_[old->second.name].erase(og);
        }
        fields_[og].name = name;
        name_to_fields_[name].insert(og);
    }

    for (auto const& kid: Kids) {
        if (bad_fields_.contains(kid)) {
            continue;
        }

        if (!traverseField(kid, field, 1 + depth)) {
            bad_fields_.insert(kid);
        }
    }
    return true;
}

bool
QPDFAcroFormDocumentHelper::getNeedAppearances()
{
    return m->getNeedAppearances();
}

bool
AcroForm::getNeedAppearances()
{
    bool result = false;
    QPDFObjectHandle acroform = qpdf.getRoot().getKey("/AcroForm");
    if (acroform.isDictionary() && acroform.getKey("/NeedAppearances").isBool()) {
        result = acroform.getKey("/NeedAppearances").getBoolValue();
    }
    return result;
}

void
QPDFAcroFormDocumentHelper::setNeedAppearances(bool val)
{
    m->setNeedAppearances(val);
}

void
AcroForm::setNeedAppearances(bool val)
{
    QPDFObjectHandle acroform = qpdf.getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        qpdf.getRoot().warn(
            "ignoring call to QPDFAcroFormDocumentHelper::setNeedAppearances"
            " on a file that lacks an /AcroForm dictionary");
        return;
    }
    if (val) {
        acroform.replaceKey("/NeedAppearances", QPDFObjectHandle::newBool(true));
    } else {
        acroform.removeKey("/NeedAppearances");
    }
}

void
QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded()
{
    m->generateAppearancesIfNeeded();
}

void
AcroForm::generateAppearancesIfNeeded()
{
    if (!getNeedAppearances()) {
        return;
    }

    for (auto const& page: QPDFPageDocumentHelper(qpdf).getAllPages()) {
        for (auto& aoh: getWidgetAnnotationsForPage(page)) {
            QPDFFormFieldObjectHelper ffh = getFieldForAnnotation(aoh);
            if (ffh.getFieldType() == "/Btn") {
                // Rather than generating appearances for button fields, rely on what's already
                // there. Just make sure /AS is consistent with /V, which we can do by resetting the
                // value of the field back to itself. This code is referenced in a comment in
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
QPDFAcroFormDocumentHelper::disableDigitalSignatures()
{
    m->disableDigitalSignatures();
}

void
AcroForm::disableDigitalSignatures()
{
    qpdf.removeSecurityRestrictions();
    std::set<QPDFObjGen> to_remove;
    auto fields = getFormFields();
    for (auto& f: fields) {
        auto ft = f.getFieldType();
        if (ft == "/Sig") {
            auto oh = f.getObjectHandle();
            to_remove.insert(oh.getObjGen());
            // Make this no longer a form field. If it's also an annotation, the annotation will
            // survive. If it's only a field and is no longer referenced, it will disappear.
            oh.removeKey("/FT");
            // Remove fields that are specific to signature fields.
            oh.removeKey("/V");
            oh.removeKey("/SV");
            oh.removeKey("/Lock");
        }
    }
    removeFormFields(to_remove);
}

void
AcroForm::adjustInheritedFields(
    QPDFObjectHandle obj,
    bool override_da,
    std::string const& from_default_da,
    bool override_q,
    int from_default_q)
{
    if (!(override_da || override_q)) {
        return;
    }
    // Override /Q or /DA if needed. If this object has a field type, directly or inherited, it is a
    // field and not just an annotation. In that case, we need to override if we are getting a value
    // from the document that is different from the value we would have gotten from the old
    // document. We must take care not to override an explicit value. It's possible that /FT may be
    // inherited by lower fields that may explicitly set /DA or /Q or that this is a field whose
    // type does not require /DA or /Q and we may be put a value on the field that is unused. This
    // is harmless, so it's not worth trying to work around.

    auto has_explicit = [](QPDFFormFieldObjectHelper& field, std::string const& key) {
        if (field.getObjectHandle().hasKey(key)) {
            return true;
        }
        return !field.getInheritableFieldValue(key).null();
    };

    if (override_da || override_q) {
        QPDFFormFieldObjectHelper cur_field(obj);
        if (override_da && (!has_explicit(cur_field, "/DA"))) {
            std::string da = cur_field.getDefaultAppearance();
            if (da != from_default_da) {
                obj.replaceKey("/DA", QPDFObjectHandle::newUnicodeString(from_default_da));
            }
        }
        if (override_q && (!has_explicit(cur_field, "/Q"))) {
            int q = cur_field.getQuadding();
            if (q != from_default_q) {
                obj.replaceKey("/Q", QPDFObjectHandle::newInteger(from_default_q));
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
            std::map<std::string, std::map<std::string, std::string>> const& dr_map,
            std::map<std::string, std::map<std::string, std::vector<size_t>>> const& rnames);
        ~ResourceReplacer() override = default;
        void handleToken(QPDFTokenizer::Token const&) override;

      private:
        size_t offset{0};
        std::map<std::string, std::map<size_t, std::string>> to_replace;
    };
} // namespace

ResourceReplacer::ResourceReplacer(
    std::map<std::string, std::map<std::string, std::string>> const& dr_map,
    std::map<std::string, std::map<std::string, std::vector<size_t>>> const& rnames)
{
    // We have:
    // * dr_map[resource_type][key] == new_key
    // * rnames[resource_type][key] == set of offsets
    //
    // We want:
    // * to_replace[key][offset] = new_key

    for (auto const& [rtype, key_offsets]: rnames) {
        auto dr_map_rtype = dr_map.find(rtype);
        if (dr_map_rtype == dr_map.end()) {
            continue;
        }
        for (auto const& [old_key, offsets]: key_offsets) {
            auto dr_map_rtype_old = dr_map_rtype->second.find(old_key);
            if (dr_map_rtype_old == dr_map_rtype->second.end()) {
                continue;
            }
            for (auto const& offs: offsets) {
                to_replace[old_key][offs] = dr_map_rtype_old->second;
            }
        }
    }
}

void
ResourceReplacer::handleToken(QPDFTokenizer::Token const& token)
{
    if (token.getType() == QPDFTokenizer::tt_name) {
        auto it1 = to_replace.find(token.getValue());
        if (it1 != to_replace.end()) {
            auto it2 = it1->second.find(offset);
            if (it2 != it1->second.end()) {
                QTC::TC("qpdf", "QPDFAcroFormDocumentHelper replaced DA token");
                write(it2->second);
                offset += token.getRawValue().length();
                return;
            }
        }
    }
    offset += token.getRawValue().length();
    writeToken(token);
}

void
AcroForm::adjustDefaultAppearances(
    QPDFObjectHandle obj, std::map<std::string, std::map<std::string, std::string>> const& dr_map)
{
    // This method is called on a field that has been copied from another file but whose /DA still
    // refers to resources in the original file's /DR.

    // When appearance streams are generated for variable text fields (see ISO 32000 PDF spec
    // section 12.7.3.3), the field's /DA is used to generate content of the appearance stream. /DA
    // contains references to resources that may be resolved in the document's /DR dictionary, which
    // appears in the document's /AcroForm dictionary. For fields that we copied from other
    // documents, we need to ensure that resources are mapped correctly in the case of conflicting
    // names. For example, if a.pdf's /DR has /F1 pointing to one font and b.pdf's /DR also has /F1
    // but it points elsewhere, we need to make sure appearance streams of fields copied from b.pdf
    // into a.pdf use whatever font /F1 meant in b.pdf, not whatever it means in a.pdf. This method
    // takes care of that. It is only called on fields copied from foreign files.

    // A few notes:
    //
    // * If the from document's /DR and the current document's /DR have conflicting keys, we have
    //   already resolved the conflicts before calling this method. The dr_map parameter contains
    //   the mapping from old keys to new keys.
    //
    // * /DA may be inherited from the document's /AcroForm dictionary. By the time this method has
    //   been called, we have already copied any document-level values into the fields to avoid
    //   having them inherit from the new document. This was done in adjustInheritedFields.

    auto DA = obj.getKey("/DA");
    if (!DA.isString()) {
        return;
    }

    // Find names in /DA. /DA is a string that contains content stream-like code, so we create a
    // stream out of the string and then filter it. We don't attach the stream to anything, so it
    // will get discarded.
    ResourceFinder rf;
    auto da_stream = QPDFObjectHandle::newStream(&qpdf, DA.getUTF8Value());
    try {
        auto nwarnings = qpdf.numWarnings();
        da_stream.parseAsContents(&rf);
        if (qpdf.numWarnings() > nwarnings) {
            QTC::TC("qpdf", "QPDFAcroFormDocumentHelper /DA parse error");
        }
    } catch (std::exception& e) {
        // No way to reproduce in test suite right now since error conditions are converted to
        // warnings.
        obj.warn("Unable to parse /DA: "s + e.what() + "; this form field may not update properly");
        return;
    }

    // Regenerate /DA by filtering its tokens.
    ResourceReplacer rr(dr_map, rf.getNamesByResourceType());
    Pl_Buffer buf_pl("filtered DA");
    da_stream.filterAsContents(&rr, &buf_pl);
    std::string new_da = buf_pl.getString();
    obj.replaceKey("/DA", QPDFObjectHandle::newString(new_da));
}

void
AcroForm::adjustAppearanceStream(
    QPDFObjectHandle stream, std::map<std::string, std::map<std::string, std::string>> dr_map)
{
    // We don't have to modify appearance streams or their resource dictionaries for them to display
    // properly, but we need to do so to make them save to regenerate. Suppose an appearance stream
    // as a font /F1 that is different from /F1 in /DR, and that when we copy the field, /F1 is
    // remapped to /F1_1. When the field is regenerated, /F1_1 won't appear in the stream's resource
    // dictionary, so the regenerated appearance stream will revert to the /F1_1 in /DR. If we
    // adjust existing appearance streams, we are protected from this problem.

    auto dict = stream.getDict();
    auto resources = dict.getKey("/Resources");

    // Make sure this stream has its own private resource dictionary.
    bool was_indirect = resources.isIndirect();
    resources = resources.shallowCopy();
    if (was_indirect) {
        resources = qpdf.makeIndirectObject(resources);
    }
    dict.replaceKey("/Resources", resources);
    // Create a dictionary with top-level keys so we can use mergeResources to force them to be
    // unshared. We will also use this to resolve conflicts that may already be in the resource
    // dictionary.
    auto merge_with = QPDFObjectHandle::newDictionary();
    for (auto const& top_key: dr_map) {
        merge_with.replaceKey(top_key.first, QPDFObjectHandle::newDictionary());
    }
    resources.mergeResources(merge_with);
    // Rename any keys in the resource dictionary that we remapped.
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
            if (!existing_new.null()) {
                // The resource dictionary already has a key in it matching what we remapped an old
                // key to, so we'll have to move it out of the way. Stick it in merge_with, which we
                // will re-merge with the dictionary when we're done. We know merge_with already has
                // dictionaries for all the top keys.
                merge_with.getKey(top_key).replaceKey(new_key, existing_new);
            }
            auto existing_old = subdict.getKey(old_key);
            if (!existing_old.null()) {
                subdict.replaceKey(new_key, existing_old);
                subdict.removeKey(old_key);
            }
        }
    }
    // Deal with any any conflicts by re-merging with merge_with and updating our local copy of
    // dr_map, which we will use to modify the stream contents.
    resources.mergeResources(merge_with, &dr_map);
    // Remove empty subdictionaries
    for (auto iter: resources.ditems()) {
        if (iter.second.isDictionary() && iter.second.getKeys().empty()) {
            resources.removeKey(iter.first);
        }
    }

    // Now attach a token filter to replace the actual resources.
    ResourceFinder rf;
    try {
        auto nwarnings = qpdf.numWarnings();
        stream.parseAsContents(&rf);
        QTC::TC(
            "qpdf",
            "QPDFAcroFormDocumentHelper AP parse error",
            qpdf.numWarnings() > nwarnings ? 0 : 1);
        auto rr = new ResourceReplacer(dr_map, rf.getNamesByResourceType());
        auto tf = std::shared_ptr<QPDFObjectHandle::TokenFilter>(rr);
        stream.addTokenFilter(tf);
    } catch (std::exception& e) {
        // No way to reproduce in test suite right now since error conditions are converted to
        // warnings.
        stream.warn("Unable to parse appearance stream: "s + e.what());
    }
}

void
QPDFAcroFormDocumentHelper::transformAnnotations(
    QPDFObjectHandle a_old_annots,
    std::vector<QPDFObjectHandle>& new_annots,
    std::vector<QPDFObjectHandle>& new_fields,
    std::set<QPDFObjGen>& old_fields,
    QPDFMatrix const& cm,
    QPDF* from_qpdf,
    QPDFAcroFormDocumentHelper* from_afdh)
{
    if (!from_qpdf) {
        // Assume these are from the same QPDF.
        from_qpdf = &qpdf;
        from_afdh = this;
    } else if (from_qpdf != &qpdf && !from_afdh) {
        from_afdh = &QPDFAcroFormDocumentHelper::get(*from_qpdf);
    }
    m->transformAnnotations(
        a_old_annots, new_annots, new_fields, old_fields, cm, from_qpdf, from_afdh->m.get());
}

void
AcroForm::transformAnnotations(
    QPDFObjectHandle a_old_annots,
    std::vector<QPDFObjectHandle>& new_annots,
    std::vector<QPDFObjectHandle>& new_fields,
    std::set<QPDFObjGen>& old_fields,
    QPDFMatrix const& cm,
    QPDF* from_qpdf,
    AcroForm* from_afdh)
{
    qpdf_expect(from_qpdf);
    qpdf_expect(from_afdh);
    Array old_annots = std::move(a_old_annots);
    const bool foreign = from_qpdf != &qpdf;

    // It's possible that we will transform annotations that don't include any form fields. This
    // code takes care not to muck around with /AcroForm unless we have to.

    Dictionary acroform = qpdf.getRoot()["/AcroForm"];
    Dictionary from_acroform = from_qpdf->getRoot()["/AcroForm"];

    // /DA and /Q may be inherited from the document-level /AcroForm dictionary. If we are copying a
    // foreign stream and the stream is getting one of these values from its document's /AcroForm,
    // we will need to copy the value explicitly so that it doesn't start getting its default from
    // the destination document.
    std::string from_default_da;
    int from_default_q = 0;
    // If we copy any form fields, we will need to merge the source document's /DR into this
    // document's /DR.
    Dictionary from_dr;
    std::string default_da;
    int default_q = 0;
    if (foreign) {
        if (acroform) {
            if (acroform["/DA"].isString()) {
                default_da = acroform["/DA"].getUTF8Value();
            }
            if (Integer Q = acroform["/Q"]) {
                default_q = Q;
            }
        }
        if (from_acroform) {
            from_dr = from_acroform["/DR"];
            if (from_dr) {
                if (!from_dr.indirect()) {
                    from_dr = from_qpdf->makeIndirectObject(from_dr);
                }
                from_dr = qpdf.copyForeignObject(from_dr);
            }
            if (from_acroform["/DA"].isString()) {
                from_default_da = from_acroform["/DA"].getUTF8Value();
            }
            if (Integer Q = from_acroform["/Q"]) {
                from_default_q = Q;
            }
        }
    }
    const bool override_da = from_acroform ? from_default_da != default_da : false;
    const bool override_q = from_acroform ? from_default_q != default_q : false;

    // If we have to merge /DR, we will need a mapping of conflicting keys for rewriting /DA. Set
    // this up for lazy initialization in case we encounter any form fields.
    std::map<std::string, std::map<std::string, std::string>> dr_map;
    Dictionary dr;

    auto init_dr_map = [&]() {
        if (!dr) {
            // Ensure that we have a /DR that is an indirect
            // dictionary object.
            if (!acroform) {
                acroform = getOrCreateAcroForm();
            }
            dr = acroform["/DR"];
            if (!dr) {
                dr = Dictionary::empty();
            }
            QPDFObjectHandle(dr).makeResourcesIndirect(qpdf);
            if (!dr.indirect()) {
                acroform.replace("/DR", qpdf.makeIndirectObject(dr));
                dr = acroform["/DR"];
            }
            // Merge the other document's /DR, creating a conflict map. mergeResources checks to
            // make sure both objects are dictionaries. By this point, if this is foreign, from_dr
            // has been copied, so we use the target qpdf as the owning qpdf.
            QPDFObjectHandle(from_dr).makeResourcesIndirect(qpdf);
            QPDFObjectHandle(dr).mergeResources(from_dr, &dr_map);

            if (from_afdh->getNeedAppearances()) {
                setNeedAppearances(true);
            }
        }
    };

    // This helper prevents us from copying the same object multiple times.
    std::map<QPDFObjGen, QPDFObjectHandle> orig_to_copy;
    auto maybe_copy_object = [&](QPDFObjectHandle& to_copy) {
        auto og = to_copy.getObjGen();
        if (orig_to_copy.contains(og)) {
            to_copy = orig_to_copy[og];
            return false;
        } else {
            to_copy = qpdf.makeIndirectObject(to_copy.shallowCopy());
            orig_to_copy[og] = to_copy;
            return true;
        }
    };

    // Traverse the field, copying kids, and preserving integrity.
    auto traverse_field = [&](QPDFObjectHandle& top_field) -> void {
        std::deque<Dictionary> queue({top_field});
        QPDFObjGen::set seen;
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            auto& obj = *it;
            if (seen.add(obj)) {
                Dictionary parent = obj["/Parent"];
                if (parent.indirect()) {
                    auto parent_og = parent.id_gen();
                    if (orig_to_copy.contains(parent_og)) {
                        obj.replace("/Parent", orig_to_copy[parent_og]);
                    } else {
                        parent.warn(
                            "while traversing field " + obj.id_gen().unparse(',') +
                            ", found parent (" + parent_og.unparse(',') +
                            ") that had not been seen, indicating likely invalid field structure");
                    }
                }
                size_t i = 0;
                Array Kids = obj["/Kids"];
                for (auto& kid: Kids) {
                    if (maybe_copy_object(kid)) {
                        Kids.set(i, kid);
                        queue.emplace_back(kid);
                    }
                    ++i;
                }
                adjustInheritedFields(
                    obj, override_da, from_default_da, override_q, from_default_q);
                if (foreign) {
                    // Lazily initialize our /DR and the conflict map.
                    init_dr_map();
                    // The spec doesn't say anything about /DR on the field, but lots of writers
                    // put one there, and it is frequently the same as the document-level /DR.
                    // To avoid having the field's /DR point to information that we are not
                    // maintaining, just reset it to that if it exists. Empirical evidence
                    // suggests that many readers, including Acrobat, Adobe Acrobat Reader,
                    // chrome, firefox, the mac Preview application, and several of the free
                    // readers on Linux all ignore /DR at the field level.
                    if (obj.contains("/DR")) {
                        obj.replace("/DR", dr);
                    }
                    if (obj["/DA"].isString() && !dr_map.empty()) {
                        adjustDefaultAppearances(obj, dr_map);
                    }
                }
            }
        }
    };

    auto transform_annotation =
        [&](QPDFObjectHandle& annot) -> std::tuple<QPDFObjectHandle, bool, bool> {
        // Make copies of annotations and fields down to the appearance streams, preserving all
        // internal referential integrity. When the incoming annotations are from a different file,
        // we first copy them locally. Then, whether local or foreign, we copy them again so that if
        // we bring the same annotation in multiple times (e.g. overlaying a foreign page onto
        // multiple local pages or a local page onto multiple other local pages), we don't create
        // annotations that are referenced in more than one place. If we did that, the effect of
        // applying transformations would be cumulative, which is definitely not what we want.
        // Besides, annotations and fields are not intended to be referenced in multiple places.

        // Determine if this annotation is attached to a form field. If so, the annotation may be
        // the same object as the form field, or the form field may have the annotation as a kid. In
        // either case, we have to walk up the field structure to find the top-level field. Within
        // one iteration through a set of annotations, we don't want to copy the same item more than
        // once. For example, suppose we have field A with kids B, C, and D, each of which has
        // annotations BA, CA, and DA. When we get to BA, we will find that BA is a kid of B which
        // is under A. When we do a copyForeignObject of A, it will also copy everything else
        // because of the indirect references. When we clone BA, we will want to clone A and then
        // update A's clone's kid to point B's clone and B's clone's parent to point to A's clone.
        // The same thing holds for annotations. Next, when we get to CA, we will again discover
        // that A is the top, but we don't want to re-copy A. We want CA's clone to be linked to the
        // same clone as BA's. Failure to do this will break up things like radio button groups,
        // which all have to kids of the same parent.

        auto ffield = from_afdh->getFieldForAnnotation(annot);
        auto ffield_oh = ffield.getObjectHandle();
        if (ffield.null()) {
            return {{}, false, false};
        }
        if (ffield_oh.isStream()) {
            ffield.warn("ignoring form field that's a stream");
            return {{}, false, false};
        }
        if (!ffield_oh.isIndirect()) {
            ffield.warn("ignoring form field not indirect");
            return {{}, false, false};
        }
        // A field and its associated annotation can be the same object. This matters because we
        // don't want to clone the annotation and field separately in this case.
        // Find the top-level field. It may be the field itself.
        bool have_parent = false;
        QPDFObjectHandle top_field = ffield.getTopLevelField(&have_parent).getObjectHandle();
        if (foreign) {
            // copyForeignObject returns the same value if called multiple times with the same
            // field. Create/retrieve the local copy of the original field. This pulls over
            // everything the field references including annotations and appearance streams, but
            // it's harmless to call copyForeignObject on them too. They will already be copied,
            // so we'll get the right object back.

            // top_field and ffield_oh are known to be indirect.
            top_field = qpdf.copyForeignObject(top_field);
            ffield_oh = qpdf.copyForeignObject(ffield_oh);
        } else {
            // We don't need to add top_field to old_fields if it's foreign because the new copy
            // of the foreign field won't be referenced anywhere. It's just the starting point
            // for us to make an additional local copy of.
            old_fields.insert(top_field.getObjGen());
        }

        if (maybe_copy_object(top_field)) {
            traverse_field(top_field);
        }

        // Now switch to copies. We already switched for top_field
        maybe_copy_object(ffield_oh);
        return {top_field, true, have_parent};
    };

    // Now do the actual copies.

    QPDFObjGen::set added_new_fields;
    for (auto annot: old_annots) {
        if (annot.isStream()) {
            annot.warn("ignoring annotation that's a stream");
            continue;
        }
        auto [top_field, have_field, have_parent] = transform_annotation(annot);

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
            annot = qpdf.copyForeignObject(annot);
        }
        maybe_copy_object(annot);

        // Now we have copies, so we can safely mutate.
        if (have_field && added_new_fields.add(top_field)) {
            new_fields.emplace_back(top_field);
        }
        new_annots.emplace_back(annot);

        // Identify and copy any appearance streams

        auto ah = QPDFAnnotationObjectHelper(annot);
        Dictionary apdict = ah.getAppearanceDictionary();
        std::vector<QPDFObjectHandle> streams;
        auto replace_stream = [](auto& dict, auto& key, auto& old) {
            dict.replace(key, old.copyStream());
            return dict[key];
        };

        for (auto& [key1, value1]: apdict) {
            if (value1.isStream()) {
                streams.emplace_back(replace_stream(apdict, key1, value1));
            } else {
                for (auto& [key2, value2]: Dictionary(value1)) {
                    if (value2.isStream()) {
                        streams.emplace_back(replace_stream(value1, key2, value2));
                    }
                }
            }
        }

        // Now we can safely mutate the annotation and its appearance streams.
        for (auto& stream: streams) {
            Dictionary dict = stream.getDict();

            QPDFMatrix apcm;
            Array omatrix = dict["/Matrix"];
            if (omatrix) {
                apcm = QPDFMatrix(QPDFObjectHandle(omatrix).getArrayAsMatrix());
            }
            apcm.concat(cm);
            if (omatrix || apcm != QPDFMatrix()) {
                dict.replace("/Matrix", QPDFObjectHandle::newFromMatrix(apcm));
            }
            Dictionary resources = dict["/Resources"];
            if (!dr_map.empty() && resources) {
                adjustAppearanceStream(stream, dr_map);
            }
        }
        auto rect = cm.transformRectangle(annot["/Rect"].getArrayAsRectangle());
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
    m->fixCopiedAnnotations(to_page, from_page, *from_afdh.m, added_fields);
}

void
AcroForm::fixCopiedAnnotations(
    QPDFObjectHandle to_page,
    QPDFObjectHandle from_page,
    AcroForm& from_afdh,
    std::set<QPDFObjGen>* added_fields)
{
    auto old_annots = from_page.getKey("/Annots");
    if (old_annots.empty() || !old_annots.isArray()) {
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
        &(from_afdh.qpdf),
        &from_afdh);

    to_page.replaceKey("/Annots", QPDFObjectHandle::newArray(new_annots));
    addAndRenameFormFields(new_fields);
    if (added_fields) {
        for (auto const& f: new_fields) {
            added_fields->insert(f.getObjGen());
        }
    }
}

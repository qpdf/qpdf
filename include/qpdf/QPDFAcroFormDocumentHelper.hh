// Copyright (c) 2005-2022 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef QPDFACROFORMDOCUMENTHELPER_HH
#define QPDFACROFORMDOCUMENTHELPER_HH

// This document helper is intended to help with operations on
// interactive forms. Here are the key things to know:

// * The PDF specification talks about interactive forms and also
//   about form XObjects. While form XObjects appear in parts of
//   interactive forms, this class is concerned about interactive
//   forms, not form XObjects.
//
// * Interactive forms are discussed in the PDF Specification (ISO PDF
//   32000-1:2008) section 12.7. Also relevant is the section about
//   Widget annotations. Annotations are discussed in section 12.5
//   with annotation dictionaries discussed in 12.5.1. Widget
//   annotations are discussed specifically in section 12.5.6.19.
//
// * What you need to know about the structure of interactive forms in
//   PDF files:
//
//   - The document catalog contains the key "/AcroForm" which
//     contains a list of fields. Fields are represented as a tree
//     structure much like pages. Nodes in the fields tree may contain
//     other fields. Fields may inherit values of many of their
//     attributes from ancestors in the tree.
//
//   - Fields may also have children that are widget annotations. As a
//     special case, and a cause of considerable confusion, if a field
//     has a single annotation as a child, the annotation dictionary
//     may be merged with the field dictionary. In that case, the
//     field and the annotation are in the same object. Note that,
//     while field dictionary attributes are inherited, annotation
//     dictionary attributes are not.
//
//   - A page dictionary contains a key called "/Annots" which
//     contains a simple list of annotations. For any given annotation
//     of subtype "/Widget", you should encounter that annotation in
//     the "/Annots" dictionary of a page, and you should also be able
//     to reach it by traversing through the "/AcroForm" dictionary
//     from the document catalog. In the simplest case (and also a
//     very common case), a form field's widget annotation will be
//     merged with the field object, and the object will appear
//     directly both under "/Annots" in the page dictionary and under
//     "/Fields" in the "/AcroForm" dictionary. In a more complex
//     case, you may have to trace through various "/Kids" elements in
//     the "/AcroForm" field entry until you find the annotation
//     dictionary.

#include <qpdf/QPDFDocumentHelper.hh>

#include <qpdf/DLL.h>

#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QPDFFormFieldObjectHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

#include <map>
#include <set>
#include <vector>

class QPDFAcroFormDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFAcroFormDocumentHelper(QPDF&);
    QPDF_DLL
    virtual ~QPDFAcroFormDocumentHelper() = default;

    // This class lazily creates an internal cache of the mapping
    // among form fields, annotations, and pages. Methods within this
    // class preserve the validity of this cache. However, if you
    // modify pages' annotation dictionaries, the document's /AcroForm
    // dictionary, or any form fields manually in a way that alters
    // the association between forms, fields, annotations, and pages,
    // it may cause this cache to become invalid. This method marks
    // the cache invalid and forces it to be regenerated the next time
    // it is needed.
    QPDF_DLL
    void invalidateCache();

    QPDF_DLL
    bool hasAcroForm();

    // Add a form field, initializing the document's AcroForm
    // dictionary if needed, updating the cache if necessary. Note
    // that you are adding fields that are copies of other fields,
    // this method may result in multiple fields existing with the
    // same qualified name, which can have unexpected side effects. In
    // that case, you should use addAndRenameFormFields() instead.
    QPDF_DLL
    void addFormField(QPDFFormFieldObjectHelper);

    // Add a collection of form fields making sure that their fully
    // qualified names don't conflict with already present form
    // fields. Fields within the collection of new fields that have
    // the same name as each other will continue to do so.
    QPDF_DLL
    void addAndRenameFormFields(std::vector<QPDFObjectHandle> fields);

    // Remove fields from the fields array
    QPDF_DLL
    void removeFormFields(std::set<QPDFObjGen> const&);

    // Set the name of a field, updating internal records of field
    // names. Name should be UTF-8 encoded.
    QPDF_DLL
    void setFormFieldName(QPDFFormFieldObjectHelper, std::string const& name);

    // Return a vector of all terminal fields in a document. Terminal
    // fields are fields that have no children that are also fields.
    // Terminal fields may still have children that are annotations.
    // Intermediate nodes in the fields tree are not included in this
    // list, but you can still reach them through the getParent method
    // of the field object helper.
    QPDF_DLL
    std::vector<QPDFFormFieldObjectHelper> getFormFields();

    // Return all the form fields that have the given fully-qualified
    // name and also have an explicit "/T" attribute. For this
    // information to be accurate, any changes to field names must be
    // done through setFormFieldName() above.
    QPDF_DLL
    std::set<QPDFObjGen> getFieldsWithQualifiedName(std::string const& name);

    // Return the annotations associated with a terminal field. Note
    // that in the case of a field having a single annotation, the
    // underlying object will typically be the same as the underlying
    // object for the field.
    QPDF_DLL
    std::vector<QPDFAnnotationObjectHelper>
        getAnnotationsForField(QPDFFormFieldObjectHelper);

    // Return annotations of subtype /Widget for a page.
    QPDF_DLL
    std::vector<QPDFAnnotationObjectHelper>
        getWidgetAnnotationsForPage(QPDFPageObjectHelper);

    // Return top-level form fields for a page.
    QPDF_DLL
    std::vector<QPDFFormFieldObjectHelper>
        getFormFieldsForPage(QPDFPageObjectHelper);

    // Return the terminal field that is associated with this
    // annotation. If the annotation dictionary is merged with the
    // field dictionary, the underlying object will be the same, but
    // this is not always the case. Note that if you call this method
    // with an annotation that is not a widget annotation, there will
    // not be an associated field, and this method will return a
    // helper associated with a null object (isNull() == true).
    QPDF_DLL
    QPDFFormFieldObjectHelper getFieldForAnnotation(QPDFAnnotationObjectHelper);

    // Return the current value of /NeedAppearances. If
    // /NeedAppearances is missing, return false as that is how PDF
    // viewers are supposed to interpret it.
    QPDF_DLL
    bool getNeedAppearances();

    // Indicate whether appearance streams must be regenerated. If you
    // modify a field value, you should call setNeedAppearances(true)
    // unless you also generate an appearance stream for the
    // corresponding annotation at the same time. If you generate
    // appearance streams for all fields, you can call
    // setNeedAppearances(false). If you use
    // QPDFFormFieldObjectHelper::setV, it will automatically call
    // this method unless you tell it not to.
    QPDF_DLL
    void setNeedAppearances(bool);

    // If /NeedAppearances is false, do nothing. Otherwise generate
    // appearance streams for all widget annotations that need them.
    // See comments in QPDFFormFieldObjectHelper.hh for
    // generateAppearance for limitations. For checkbox and radio
    // button fields, this code ensures that appearance state is
    // consistent with the field's value and uses any pre-existing
    // appearance streams.
    QPDF_DLL
    void generateAppearancesIfNeeded();

    // Note: this method works on all annotations, not just ones with
    // associated fields. For each annotation in old_annots, apply the
    // given transformation matrix to create a new annotation. New
    // annotations are appended to new_annots. If the annotation is
    // associated with a form field, a new form field is created that
    // points to the new annotation and is appended to new_fields, and
    // the old field is added to old_fields.
    //
    // old_annots may belong to a different QPDF object. In that case,
    // you should pass in from_qpdf, and copyForeignObject will be
    // called automatically. If this is the case, for efficiency, you
    // may pass in a QPDFAcroFormDocumentHelper for the other file to
    // avoid the expensive process of creating one for each call to
    // transformAnnotations. New fields and annotations are not added
    // to the document or pages. You have to do that yourself after
    // calling transformAnnotations. If this operation will leave
    // orphaned fields behind, such as if you are replacing the old
    // annotations with the new ones on the same page and the fields
    // and annotations are not shared, you will also need to remove
    // the old fields to prevent them from hanging round unreferenced.
    QPDF_DLL
    void transformAnnotations(
        QPDFObjectHandle old_annots,
        std::vector<QPDFObjectHandle>& new_annots,
        std::vector<QPDFObjectHandle>& new_fields,
        std::set<QPDFObjGen>& old_fields,
        QPDFMatrix const& cm,
        QPDF* from_qpdf = nullptr,
        QPDFAcroFormDocumentHelper* from_afdh = nullptr);

    // Copy form fields and annotations from one page to another,
    // allowing the from page to be in a different QPDF or in the same
    // QPDF. This would typically be called after calling addPage to
    // add field/annotation awareness. When just copying the page by
    // itself, annotations end up being shared, and fields end up
    // being omitted because there is no reference to the field from
    // the page. This method ensures that each separate copy of a page
    // has private annotations and that fields and annotations are
    // properly updated to resolve conflicts that may occur from
    // common resource and field names across documents. It is
    // basically a wrapper around transformAnnotations that handles
    // updating the receiving page. If new_fields is non-null, any
    // newly created fields are added to it.
    QPDF_DLL
    void fixCopiedAnnotations(
        QPDFObjectHandle to_page,
        QPDFObjectHandle from_page,
        QPDFAcroFormDocumentHelper& from_afdh,
        std::set<QPDFObjGen>* new_fields = nullptr);

  private:
    void analyze();
    void traverseField(
        QPDFObjectHandle field,
        QPDFObjectHandle parent,
        int depth,
        std::set<QPDFObjGen>& visited);
    QPDFObjectHandle getOrCreateAcroForm();
    void adjustInheritedFields(
        QPDFObjectHandle obj,
        bool override_da,
        std::string const& from_default_da,
        bool override_q,
        int from_default_q);
    void adjustDefaultAppearances(
        QPDFObjectHandle obj,
        std::map<std::string, std::map<std::string, std::string>> const&
            dr_map);
    void adjustAppearanceStream(
        QPDFObjectHandle stream,
        std::map<std::string, std::map<std::string, std::string>> dr_map);

    class Members
    {
        friend class QPDFAcroFormDocumentHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members();
        Members(Members const&) = delete;

        bool cache_valid;
        std::map<QPDFObjGen, std::vector<QPDFAnnotationObjectHelper>>
            field_to_annotations;
        std::map<QPDFObjGen, QPDFFormFieldObjectHelper> annotation_to_field;
        std::map<QPDFObjGen, std::string> field_to_name;
        std::map<std::string, std::set<QPDFObjGen>> name_to_fields;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFACROFORMDOCUMENTHELPER_HH

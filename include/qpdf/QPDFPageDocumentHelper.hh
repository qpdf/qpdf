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

#ifndef QPDFPAGEDOCUMENTHELPER_HH
#define QPDFPAGEDOCUMENTHELPER_HH

#include <qpdf/Constants.h>
#include <qpdf/QPDFDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

#include <qpdf/DLL.h>

#include <vector>

#include <qpdf/QPDF.hh>

class QPDFAcroFormDocumentHelper;

class QPDFPageDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFPageDocumentHelper(QPDF&);
    QPDF_DLL
    virtual ~QPDFPageDocumentHelper() = default;

    // Traverse page tree, and return all /Page objects wrapped in
    // QPDFPageObjectHelper objects. Unlike with
    // QPDF::getAllPages, the vector of pages returned by
    // this call is not affected by additions or removals of pages. If
    // you manipulate pages, you will have to call this again to get a
    // new copy. Please see comments in QPDF.hh for getAllPages() for
    // additional details.
    QPDF_DLL
    std::vector<QPDFPageObjectHelper> getAllPages();

    // The PDF /Pages tree allows inherited values. Working with the
    // pages of a pdf is much easier when the inheritance is resolved
    // by explicitly setting the values in each /Page.
    QPDF_DLL
    void pushInheritedAttributesToPage();

    // This calls QPDFPageObjectHelper::removeUnreferencedResources
    // for every page in the document. See comments in
    // QPDFPageObjectHelper.hh for details.
    QPDF_DLL
    void removeUnreferencedResources();

    // Add new page at the beginning or the end of the current pdf.
    // The newpage parameter may be either a direct object, an
    // indirect object from this QPDF, or an indirect object from
    // another QPDF. If it is a direct object, it will be made
    // indirect. If it is an indirect object from another QPDF, this
    // method will call pushInheritedAttributesToPage on the other
    // file and then copy the page to this QPDF using the same
    // underlying code as copyForeignObject. At this stage, if the
    // indirect object is already in the pages tree, a shallow copy is
    // made to avoid adding the same page more than once. In version
    // 10.3.1 and earlier, adding a page that already existed would
    // throw an exception and could cause qpdf to crash on subsequent
    // page insertions in some cases. Note that this means that, in
    // some cases, the page actually added won't be exactly the same
    // object as the one passed in. If you want to do subsequent
    // modification on the page, you should retrieve it again.
    //
    // Note that you can call copyForeignObject directly to copy a
    // page from a different file, but the resulting object will not
    // be a page in the new file. You could do this, for example, to
    // convert a page into a form XObject, though for that, you're
    // better off using QPDFPageObjectHelper::getFormXObjectForPage.
    //
    // This method does not have any specific awareness of annotations
    // or form fields, so if you just add a page without thinking
    // about it, you might end up with two pages that share form
    // fields or annotations. While the page may look fine, it will
    // probably not function properly with regard to interactive
    // features. To work around this, you should called
    // QPDFAcroFormDocumentHelper::fixCopiedAnnotations. A future
    // version of qpdf will likely provide a higher-level interface
    // for copying pages around that will handle document-level
    // constructs in a less error-prone fashion.

    QPDF_DLL
    void addPage(QPDFPageObjectHelper newpage, bool first);

    // Add new page before or after refpage. See comments for addPage
    // for details about what newpage should be.
    QPDF_DLL
    void addPageAt(
        QPDFPageObjectHelper newpage,
        bool before,
        QPDFPageObjectHelper refpage);

    // Remove page from the pdf.
    QPDF_DLL
    void removePage(QPDFPageObjectHelper page);

    // For every annotation, integrate the annotation's appearance
    // stream into the containing page's content streams, merge the
    // annotation's resources with the page's resources, and remove
    // the annotation from the page. Handles widget annotations
    // associated with interactive form fields as a special case,
    // including removing the /AcroForm key from the document catalog.
    // The values passed to required_flags and forbidden_flags are
    // passed along to
    // QPDFAnnotationObjectHelper::getPageContentForAppearance. See
    // comments there in QPDFAnnotationObjectHelper.hh for meanings of
    // those flags.
    QPDF_DLL
    void flattenAnnotations(
        int required_flags = 0, int forbidden_flags = an_invisible | an_hidden);

  private:
    void flattenAnnotationsForPage(
        QPDFPageObjectHelper& page,
        QPDFObjectHandle& resources,
        QPDFAcroFormDocumentHelper& afdh,
        int required_flags,
        int forbidden_flags);

    class Members
    {
        friend class QPDFPageDocumentHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFPAGEDOCUMENTHELPER_HH

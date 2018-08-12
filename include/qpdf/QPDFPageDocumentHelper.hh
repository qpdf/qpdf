// Copyright (c) 2005-2018 Jay Berkenbilt
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

#include <qpdf/QPDFDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

#include <qpdf/DLL.h>

#include <vector>

#include <qpdf/QPDF.hh>

class QPDFPageDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFPageDocumentHelper(QPDF&);

    // Traverse page tree, and return all /Page objects wrapped in
    // QPDFPageObjectHelper objects. Unlike with
    // QPDFObjectHandle::getAllPages, the vector of pages returned by
    // this call is not affected by additions or removals of pages. If
    // you manipulate pages, you will have to call this again to get a
    // new copy. Please comments in QPDFObjectHandle.hh for
    // getAllPages() for additional details.
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
    // underlying code as copyForeignObject.
    QPDF_DLL
    void addPage(QPDFPageObjectHelper newpage, bool first);

    // Add new page before or after refpage. See comments for addPage
    // for details about what newpage should be.
    QPDF_DLL
    void addPageAt(QPDFPageObjectHelper newpage, bool before,
                   QPDFPageObjectHelper refpage);

    // Remove page from the pdf.
    QPDF_DLL
    void removePage(QPDFPageObjectHelper page);

  private:
    class Members
    {
        friend class QPDFPageDocumentHelper;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&);
    };

    PointerHolder<Members> m;
};

#endif // QPDFPAGEDOCUMENTHELPER_HH

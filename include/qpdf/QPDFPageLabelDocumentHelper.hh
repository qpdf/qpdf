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

#ifndef QPDFPAGELABELDOCUMENTHELPER_HH
#define QPDFPAGELABELDOCUMENTHELPER_HH

#include <qpdf/QPDFDocumentHelper.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <vector>

#include <qpdf/DLL.h>

// Page labels are discussed in the PDF spec (ISO-32000) in section
// 12.4.2.
//
// Page labels are implemented as a number tree. Each key is a page
// index, numbered from 0. The values are dictionaries with the
// following keys, all optional:
//
// * /Type: if present, must be /PageLabel
// * /S: one of /D, /R, /r, /A, or /a for decimal, upper-case and
//   lower-case Roman numeral, or upper-case and lower-case alphabetic
// * /P: if present, a fixed prefix string that is prepended to each
//   page number
// * /St: the starting number, or 1 if not specified

class QPDFPageLabelDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFPageLabelDocumentHelper(QPDF&);
    QPDF_DLL
    virtual ~QPDFPageLabelDocumentHelper() = default;

    QPDF_DLL
    bool hasPageLabels();

    // Return a page label dictionary representing the page label for
    // the given page. The page does not need to appear explicitly in
    // the page label dictionary. This method will adjust /St as
    // needed to produce a label that is suitable for the page.
    QPDF_DLL
    QPDFObjectHandle getLabelForPage(long long page_idx);

    // Append to the incoming vector a list of objects suitable for
    // inclusion in a /PageLabels dictionary's /Nums field. start_idx
    // and end_idx are the indexes to the starting and ending pages
    // (inclusive) in the original file, and new_start_idx is the
    // index to the first page in the new file. For example, if pages
    // 10 through 12 of one file are being copied to a new file as
    // pages 6 through 8, you would call getLabelsForPageRange(10, 12,
    // 6), which would return as many entries as are required to add
    // to the new file's PageLabels. This method fabricates a suitable
    // entry even if the original document has no page labels. This
    // behavior facilitates using this function to incrementally build
    // up a page labels tree when merging files.
    QPDF_DLL
    void getLabelsForPageRange(
        long long start_idx,
        long long end_idx,
        long long new_start_idx,
        std::vector<QPDFObjectHandle>& new_labels);

  private:
    class Members
    {
        friend class QPDFPageLabelDocumentHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;

        std::shared_ptr<QPDFNumberTreeObjectHelper> labels;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFPAGELABELDOCUMENTHELPER_HH

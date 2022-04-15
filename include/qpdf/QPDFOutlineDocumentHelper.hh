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

#ifndef QPDFOUTLINEDOCUMENTHELPER_HH
#define QPDFOUTLINEDOCUMENTHELPER_HH

#include <qpdf/QPDFDocumentHelper.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFOutlineObjectHelper.hh>

#include <qpdf/QPDF.hh>
#include <map>
#include <set>
#include <vector>

#include <qpdf/DLL.h>

// This is a document helper for outlines, also known as bookmarks.
// Outlines are discussed in section 12.3.3 of the PDF spec
// (ISO-32000). With the help of QPDFOutlineObjectHelper, the outlines
// tree is traversed, and a bidirectional map is made between pages
// and outlines. See also QPDFOutlineObjectHelper.

class QPDFOutlineDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFOutlineDocumentHelper(QPDF&);
    QPDF_DLL
    virtual ~QPDFOutlineDocumentHelper() = default;

    QPDF_DLL
    bool hasOutlines();

    QPDF_DLL
    std::vector<QPDFOutlineObjectHelper> getTopLevelOutlines();

    // If the name is a name object, look it up in the /Dests key of
    // the document catalog. If the name is a string, look it up in
    // the name tree pointed to by the /Dests key of the names
    // dictionary.
    QPDF_DLL
    QPDFObjectHandle resolveNamedDest(QPDFObjectHandle name);

    // Return a list outlines that are known to target the specified
    // page
    QPDF_DLL
    std::vector<QPDFOutlineObjectHelper> getOutlinesForPage(QPDFObjGen const&);

    class Accessor
    {
        friend class QPDFOutlineObjectHelper;

        QPDF_DLL
        static bool
        checkSeen(QPDFOutlineDocumentHelper& dh, QPDFObjGen const& og)
        {
            return dh.checkSeen(og);
        }
    };
    friend class Accessor;

  private:
    bool checkSeen(QPDFObjGen const& og);
    void initializeByPage();

    class Members
    {
        friend class QPDFOutlineDocumentHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;

        std::vector<QPDFOutlineObjectHelper> outlines;
        std::set<QPDFObjGen> seen;
        QPDFObjectHandle dest_dict;
        std::shared_ptr<QPDFNameTreeObjectHelper> names_dest;
        std::map<QPDFObjGen, std::vector<QPDFOutlineObjectHelper>> by_page;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFOUTLINEDOCUMENTHELPER_HH

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

#ifndef QPDFOUTLINEOBJECTHELPER_HH
#define QPDFOUTLINEOBJECTHELPER_HH

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHelper.hh>
#include <vector>

class QPDFOutlineDocumentHelper;

#include <qpdf/DLL.h>

// This is an object helper for outline items. Outlines, also known as
// bookmarks, are described in section 12.3.3 of the PDF spec
// (ISO-32000). See comments below for details.

class QPDFOutlineObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    virtual ~QPDFOutlineObjectHelper()
    {
        // This must be cleared explicitly to avoid circular references
        // that prevent cleanup of pointer holders.
        this->m->parent = 0;
    }

    // All constructors are private. You can only create one of these
    // using QPDFOutlineDocumentHelper.

    // Return parent pointer. Returns a null pointer if this is a
    // top-level outline.
    QPDF_DLL
    std::shared_ptr<QPDFOutlineObjectHelper> getParent();

    // Return children as a list.
    QPDF_DLL
    std::vector<QPDFOutlineObjectHelper> getKids();

    // Return the destination, regardless of whether it is named or
    // explicit and whether it is directly provided or in a GoTo
    // action. Returns a null object if the destination can't be
    // determined. Named destinations can be resolved using the older
    // root /Dest dictionary or the current names tree.
    QPDF_DLL
    QPDFObjectHandle getDest();

    // Return the page that the outline points to. Returns a null
    // object if the destination page can't be determined.
    QPDF_DLL
    QPDFObjectHandle getDestPage();

    // Returns the value of /Count as present in the object, or 0 if
    // not present. If count is positive, the outline is open. If
    // negative, it is closed. Either way, the absolute value is the
    // number descendant items that would be visible if this were
    // open.
    QPDF_DLL
    int getCount();

    // Returns the title as a UTF-8 string. Returns the empty string
    // if there is no title.
    QPDF_DLL
    std::string getTitle();

    class Accessor
    {
        friend class QPDFOutlineDocumentHelper;

        static QPDFOutlineObjectHelper
        create(QPDFObjectHandle oh, QPDFOutlineDocumentHelper& dh, int depth)
        {
            return QPDFOutlineObjectHelper(oh, dh, depth);
        }
    };
    friend class Accessor;

  private:
    QPDF_DLL
    QPDFOutlineObjectHelper(QPDFObjectHandle, QPDFOutlineDocumentHelper&, int);

    class Members
    {
        friend class QPDFOutlineObjectHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(QPDFOutlineDocumentHelper& dh);
        Members(Members const&) = delete;

        QPDFOutlineDocumentHelper& dh;
        std::shared_ptr<QPDFOutlineObjectHelper> parent;
        std::vector<QPDFOutlineObjectHelper> kids;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFOUTLINEOBJECTHELPER_HH

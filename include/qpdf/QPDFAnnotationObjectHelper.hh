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

#ifndef QPDFANNOTATIONOBJECTHELPER_HH
#define QPDFANNOTATIONOBJECTHELPER_HH

#include <qpdf/QPDFObjectHelper.hh>

#include <qpdf/DLL.h>

class QPDFAnnotationObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFAnnotationObjectHelper(QPDFObjectHandle);

    // This class provides helper methods for certain types of
    // annotations. At its introduction, it only supports Widget
    // annotations, but other types of annotations may be supported in
    // the future. For additional information about interactive forms,
    // please see the comments at the top of
    // QPDFAcroFormDocumentHelper.hh.

    // Return the subtype of the annotation as a string (e.g.
    // "/Widget"). Returns the empty string if the subtype (which is
    // required by the spec) is missing.
    QPDF_DLL
    std::string getSubtype();

    QPDF_DLL
    QPDFObjectHandle::Rectangle getRect();

    QPDF_DLL
    QPDFObjectHandle getAppearanceDictionary();

    // Return the appearance state as given in "/AS", or the empty
    // string if none is given.
    QPDF_DLL
    std::string getAppearanceState();

    // Return a specific stream. "which" may be one of "/N", "/R", or
    // "/D" to indicate the normal, rollover, or down appearance
    // stream. (Any value may be passed to "which"; if an appearance
    // stream of that name exists, it will be returned.) If the value
    // associated with "which" in the appearance dictionary is a
    // subdictionary, an appearance state may be specified to select
    // which appearance stream is desired. If not specified, the
    // appearance state in "/AS" will used.
    QPDF_DLL
    QPDFObjectHandle getAppearanceStream(std::string const& which,
                                         std::string const& state = "");

  private:
    class Members
    {
        friend class QPDFPageObjectHelper;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&);
    };

    PointerHolder<Members> m;
};

#endif // QPDFANNOTATIONOBJECTHELPER_HH

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

#ifndef QPDFANNOTATIONOBJECTHELPER_HH
#define QPDFANNOTATIONOBJECTHELPER_HH

#include <qpdf/Constants.h>
#include <qpdf/QPDFObjectHelper.hh>

#include <qpdf/DLL.h>

class QPDFAnnotationObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFAnnotationObjectHelper(QPDFObjectHandle);
    QPDF_DLL
    virtual ~QPDFAnnotationObjectHelper() = default;

    // This class provides helper methods for annotations. More
    // functionality will likely be added in the future.

    // Some functionality for annotations is also implemented in
    // QPDFAcroFormDocumentHelper and QPDFFormFieldObjectHelper. In
    // some cases, functions defined there work for other annotations
    // besides widget annotations, but they are implemented with form
    // fields so that they can properly handle form fields when
    // needed.

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

    // Return flags from "/F". The value is a logical or of
    // pdf_annotation_flag_e as defined in qpdf/Constants.h.
    QPDF_DLL
    int getFlags();

    // Return a specific stream. "which" may be one of "/N", "/R", or
    // "/D" to indicate the normal, rollover, or down appearance
    // stream. (Any value may be passed to "which"; if an appearance
    // stream of that name exists, it will be returned.) If the value
    // associated with "which" in the appearance dictionary is a
    // subdictionary, an appearance state may be specified to select
    // which appearance stream is desired. If not specified, the
    // appearance state in "/AS" will used.
    QPDF_DLL
    QPDFObjectHandle getAppearanceStream(
        std::string const& which, std::string const& state = "");

    // Generate text suitable for addition to the containing page's
    // content stream that draws this annotation's appearance stream
    // as a form XObject. The value "name" is the resource name that
    // will be used to refer to the form xobject. The value "rotate"
    // should be set to the page's /Rotate value or 0 if none. The
    // values of required_flags and forbidden_flags are constructed by
    // logically "or"ing annotation flags of type
    // pdf_annotation_flag_e defined in qpdf/Constants.h. Content will
    // be returned only if all required_flags are set and no
    // forbidden_flags are set. For example, including an_no_view in
    // forbidden_flags could be useful for creating an on-screen view,
    // and including an_print to required_flags could be useful if
    // preparing to print.
    QPDF_DLL
    std::string getPageContentForAppearance(
        std::string const& name,
        int rotate,
        int required_flags = 0,
        int forbidden_flags = an_invisible | an_hidden);

  private:
    class Members
    {
        friend class QPDFAnnotationObjectHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFANNOTATIONOBJECTHELPER_HH

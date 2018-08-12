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

#ifndef QPDFFORMFIELDOBJECTHELPER_HH
#define QPDFFORMFIELDOBJECTHELPER_HH

// This object helper helps with form fields for interactive forms.
// Please see comments in QPDFAcroFormDocumentHelper.hh for additional
// details.

#include <qpdf/QPDFObjectHelper.hh>

#include <qpdf/DLL.h>

class QPDFFormFieldObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFFormFieldObjectHelper();
    QPDF_DLL
    QPDFFormFieldObjectHelper(QPDFObjectHandle);

    QPDF_DLL
    bool isNull();

    // Return the field's parent. A form field object helper whose
    // underlying object is null is returned if there is no parent.
    // This condition may be tested by calling isNull().
    QPDF_DLL
    QPDFFormFieldObjectHelper getParent();

    // Get a field value, possibly inheriting the value from an
    // ancestor node.
    QPDF_DLL
    QPDFObjectHandle getInheritableFieldValue(std::string const& name);

    // Get an inherited field value as a string. If it is not a
    // string, silently return the empty string.
    QPDF_DLL
    std::string getInheritableFieldValueAsString(std::string const& name);

    // Get an inherited field value of type name as a string
    // representing the name. If it is not a name, silently return
    // the empty string.
    QPDF_DLL
    std::string getInheritableFieldValueAsName(std::string const& name);

    // Returns the value of /FT if present, otherwise returns the
    // empty string.
    QPDF_DLL
    std::string getFieldType();

    QPDF_DLL
    std::string getFullyQualifiedName();

    QPDF_DLL
    std::string getPartialName();

    // Return the alternative field name (/TU), which is the field
    // name intended to be presented to users. If not present, fall
    // back to the fully qualified name.
    QPDF_DLL
    std::string getAlternativeName();

    // Return the mapping field name (/TM). If not present, fall back
    // to the alternative name, then to the partial name.
    QPDF_DLL
    std::string getMappingName();

    QPDF_DLL
    QPDFObjectHandle getValue();

    // Return the field's value as a string. If this is called with a
    // field whose value is not a string, the empty string will be
    // silently returned.
    QPDF_DLL
    std::string getValueAsString();

    QPDF_DLL
    QPDFObjectHandle getDefaultValue();

    // Return the field's default value as a string. If this is called
    // with a field whose value is not a string, the empty string will
    // be silently returned.
    QPDF_DLL
    std::string getDefaultValueAsString();

    // Return the default appearance string, taking inheritance from
    // the field tree into account. Returns the empty string if the
    // default appearance string is not available (because it's
    // erroneously absent or because this is not a variable text
    // field).
    QPDF_DLL
    std::string getDefaultAppearance();

    // Return the quadding value, taking inheritance from the field
    // tree into account. Returns 0 if quadding is not specified.
    QPDF_DLL
    int getQuadding();

    // Set an attribute to the given value
    QPDF_DLL
    void setFieldAttribute(std::string const& key, QPDFObjectHandle value);

    // Set an attribute to the given value as a Unicode string (UTF-16
    // BE encoded). The input string should be UTF-8 encoded.
    QPDF_DLL
    void setFieldAttribute(std::string const& key,
                           std::string const& utf8_value);

    // Set /V (field value) to the given value. Optionally set
    // /NeedAppearances to true. You can explicitly tell this method
    // not to set /NeedAppearances if you are going to explicitly
    // generate an appearance stream yourself.
    QPDF_DLL
    void setV(QPDFObjectHandle value, bool need_appearances = true);

    // Set /V (field value) to the given string value encoded as a
    // Unicode string. The input value should be UTF-8 encoded. See
    // comments above about /NeedAppearances.
    QPDF_DLL
    void setV(std::string const& utf8_value, bool need_appearances = true);

  private:
    class Members
    {
        friend class QPDFFormFieldObjectHelper;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&);
    };

    PointerHolder<Members> m;
};

#endif // QPDFFORMFIELDOBJECTHELPER_HH

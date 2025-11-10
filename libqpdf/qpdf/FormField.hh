#ifndef FORMFIELD_HH
#define FORMFIELD_HH

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObjectHelper.hh>

#include <vector>

class QPDFAnnotationObjectHelper;

namespace qpdf::impl
{
    // This object helper helps with form fields for interactive forms. Please see comments in
    // QPDFAcroFormDocumentHelper.hh for additional details.
    class FormField: public qpdf::BaseDictionary
    {
      public:
        FormField() = default;
        FormField(FormField const&) = default;
        FormField& operator=(FormField const&) = default;
        FormField(FormField&&) = default;
        FormField& operator=(FormField&&) = default;
        ~FormField() = default;

        FormField(QPDFObjectHandle const& oh) :
            BaseDictionary(oh)
        {
        }

        FormField(QPDFObjectHandle&& oh) :
            BaseDictionary(std::move(oh))
        {
        }

        /// Retrieves the /Parent form field of the current field.
        ///
        /// This function accesses the parent field in the hierarchical structure of form fields, if
        /// it exists. The parent is determined based on the /Parent attribute in the field
        /// dictionary.
        ///
        /// @return A FormField object representing the parent field. If the current field has no
        ///         parent, an empty FormField object is returned.
        FormField
        Parent()
        {
            return {get("/Parent")};
        }

        // Return the top-level field for this field. Typically this will be the field itself or its
        // parent. If is_different is provided, it is set to true if the top-level field is
        // different from the field itself; otherwise it is set to false.
        FormField getTopLevelField(bool* is_different = nullptr);

        // Get a field value, possibly inheriting the value from an ancestor node.
        QPDFObjectHandle getInheritableFieldValue(std::string const& name);

        // Get an inherited field value as a string. If it is not a string, silently return the
        // empty string.
        std::string getInheritableFieldValueAsString(std::string const& name);

        // Get an inherited field value of type name as a string representing the name. If it is not
        // a name, silently return the empty string.
        std::string getInheritableFieldValueAsName(std::string const& name);

        // Returns the value of /FT if present, otherwise returns the empty string.
        std::string getFieldType();

        std::string getFullyQualifiedName();

        std::string getPartialName();

        // Return the alternative field name (/TU), which is the field name intended to be presented
        // to users. If not present, fall back to the fully qualified name.
        std::string getAlternativeName();

        // Return the mapping field name (/TM). If not present, fall back to the alternative name,
        // then to the partial name.
        std::string getMappingName();

        QPDFObjectHandle getValue();

        // Return the field's value as a string. If this is called with a field whose value is not a
        std::string getValueAsString();

        QPDFObjectHandle getDefaultValue();

        // Return the field's default value as a string. If this is called with a field whose value
        // is not a string, the empty string will be silently returned.
        std::string getDefaultValueAsString();

        // Return the default appearance string, taking inheritance from the field tree into
        // account. Returns the empty string if the default appearance string is not available
        // (because it's erroneously absent or because this is not a variable text field). If not
        // found in the field hierarchy, look in /AcroForm.
        std::string getDefaultAppearance();

        // Return the default resource dictionary for the field. This comes not from the field but
        // from the document-level /AcroForm dictionary. While several PDF generates put a /DR key
        // in the form field's dictionary, experimentation suggests that many popular readers,
        // including Adobe Acrobat and Acrobat Reader, ignore any /DR item on the field.
        QPDFObjectHandle getDefaultResources();

        // Return the quadding value, taking inheritance from the field tree into account. Returns 0
        // if quadding is not specified. Look in /AcroForm if not found in the field hierarchy.
        int getQuadding();

        // Return field flags from /Ff. The value is a logical or of pdf_form_field_flag_e as
        // defined in qpdf/Constants.h//
        int getFlags();

        // Methods for testing for particular types of form fields

        // Returns true if field is of type /Tx
        bool isText();
        // Returns true if field is of type /Btn and flags do not indicate some other type of
        // button.
        bool isCheckbox();

        // Returns true if field is a checkbox and is checked.
        bool isChecked();

        // Returns true if field is of type /Btn and flags indicate that it is a radio button
        bool isRadioButton();

        // Returns true if field is of type /Btn and flags indicate that it is a pushbutton
        bool isPushbutton();

        // Returns true if fields if of type /Ch
        bool isChoice();

        // Returns choices display values as UTF-8 strings
        std::vector<std::string> getChoices();

        // Set an attribute to the given value. If you have a QPDFAcroFormDocumentHelper and you
        // want to set the name of a field, use QPDFAcroFormDocumentHelper::setFormFieldName
        // instead.
        void setFieldAttribute(std::string const& key, QPDFObjectHandle value);

        // Set an attribute to the given value as a Unicode string (UTF-16 BE encoded). The input
        // string should be UTF-8 encoded. If you have a QPDFAcroFormDocumentHelper and you want to
        // set the name of a field, use QPDFAcroFormDocumentHelper::setFormFieldName instead.
        void setFieldAttribute(std::string const& key, std::string const& utf8_value);

        // Set /V (field value) to the given value. If need_appearances is true and the field type
        // is either /Tx (text) or /Ch (choice), set /NeedAppearances to true. You can explicitly
        // tell this method not to set /NeedAppearances if you are going to generate an appearance
        // stream yourself. Starting with qpdf 8.3.0, this method handles fields of type /Btn
        // (checkboxes, radio buttons, pushbuttons) specially. When setting a checkbox value, any
        // value other than /Off will be treated as on, and the actual value set will be based on
        // the appearance stream's /N dictionary, so the value that ends up in /V may not exactly
        // match the value you pass in.
        void setV(QPDFObjectHandle value, bool need_appearances = true);

        // Set /V (field value) to the given string value encoded as a Unicode string. The input
        // value should be UTF-8 encoded. See comments above about /NeedAppearances.
        void setV(std::string const& utf8_value, bool need_appearances = true);

        // Update the appearance stream for this field. Note that qpdf's ability to generate
        // appearance streams is limited. We only generate appearance streams for streams of type
        // text or choice. The appearance uses the default parameters provided in the file, and it
        // only supports ASCII characters. Quadding is currently ignored. While this functionality
        // is limited, it should do a decent job on properly constructed PDF files when field values
        // are restricted to ASCII characters.
        void generateAppearance(QPDFAnnotationObjectHelper&);

      private:
        QPDFObjectHandle getFieldFromAcroForm(std::string const& name);
        void setRadioButtonValue(QPDFObjectHandle name);
        void setCheckBoxValue(bool value);
        void generateTextAppearance(QPDFAnnotationObjectHelper&);
        QPDFObjectHandle
        getFontFromResource(QPDFObjectHandle resources, std::string const& font_name);
    };
} // namespace qpdf::impl

#endif // FORMFIELD_HH

#ifndef ACRO_FORM_HH
#define ACRO_FORM_HH

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObjectHelper.hh>
#include <qpdf/QPDF_private.hh>

#include <vector>

class QPDFAnnotationObjectHelper;

namespace qpdf::impl
{
    class AcroForm: public Doc::Common
    {
      public:
        AcroForm() = delete;
        AcroForm(AcroForm const&) = delete;
        AcroForm(AcroForm&&) = delete;
        AcroForm& operator=(AcroForm const&) = delete;
        AcroForm& operator=(AcroForm&&) = delete;
        ~AcroForm() = default;

        AcroForm(impl::Doc& doc) :
            Common(doc)
        {
            // We have to analyze up front. Otherwise, when we are adding annotations and fields, we
            // are in a temporarily unstable configuration where some widget annotations are not
            // reachable.
            analyze();
        }

        struct FieldData
        {
            std::vector<QPDFAnnotationObjectHelper> annotations;
            std::string name;
        };

        /// Retrieves a list of widget annotations for the specified page.
        ///
        /// A widget annotation represents the visual part of a form field in a PDF.
        /// This function filters annotations on the given page, returning only those
        /// annotations whose subtype is "/Widget".
        ///
        /// @param page A `QPDFPageObjectHelper` representing the page from which to
        ///             extract widget annotations.
        ///
        /// @return A vector of `QPDFAnnotationObjectHelper` objects corresponding to
        ///         the widget annotations found on the specified page.
        std::vector<QPDFAnnotationObjectHelper>
        getWidgetAnnotationsForPage(QPDFPageObjectHelper page);

        /// Analyzes the AcroForm structure in the PDF document and updates the internal
        /// cache with the form fields and their corresponding widget annotations.
        ///
        /// The function performs the following steps:
        /// - Checks if the cache is valid. If it is, the function exits early.
        /// - Retrieves the `/AcroForm` dictionary from the PDF and checks if it contains
        ///   a `/Fields` key.
        /// - If `/Fields` exist and is an array, iterates through the fields and traverses
        ///   them to map annotations bidirectionally to form fields.
        /// - Logs a warning if the `/Fields` key is present but not an array, and initializes
        ///   it to an empty array.
        /// - Ensures that all widget annotations are processed, including any annotations
        ///   that might not be reachable from the `/AcroForm`. Treats such annotations as
        ///   their own fields.
        /// - Provides a workaround for PDF documents containing inconsistencies, such as
        ///   widget annotations on a page not being referenced in `/AcroForm`.
        ///
        /// This function allows precise navigation and manipulation of form fields and
        /// their related annotations, facilitating advanced PDF document processing.
        void analyze();

        /// Recursively traverses the structure of form fields and annotations in a PDF's /AcroForm.
        ///
        /// The method is designed to process form fields in a hierarchical /AcroForm structure.
        /// It captures field and annotation data, resolves parent-child relationships, detects
        /// loops, and avoids stack overflow from excessive recursion depth.
        ///
        /// @param field The current field or annotation to process.
        /// @param parent The parent field object. If the current field is a top-level field, parent
        ///               will be a null object.
        /// @param depth The current recursion depth to limit stack usage and avoid infinite loops.
        ///
        /// @return True if the field was processed successfully, false otherwise.
        ///
        /// - Recursion is limited to a depth of 100 to prevent stack overflow with maliciously
        ///   crafted files.
        /// - The function skips non-indirect and invalid objects (e.g., non-dictionaries or objects
        ///   with invalid parent references).
        /// - Detects and warns about loops in the /AcroForm hierarchy.
        /// - Differentiates between terminal fields, annotations, and composite fields based on
        ///   dictionary keys.
        /// - Tracks processed fields and annotations using internal maps to prevent reprocessing
        ///   and detect loops.
        /// - Updates name-to-field mappings for terminal fields with a valid fully qualified name.
        /// - Ensures the integrity of parent-child relationships within the field hierarchy.
        /// - Any invalid child objects are logged and skipped during traversal.
        bool traverseField(QPDFObjectHandle field, QPDFObjectHandle const& parent, int depth);

        /// Retrieves or creates the /AcroForm dictionary in the PDF document's root.
        ///
        /// - If the /AcroForm key exists in the document root and is a dictionary,
        ///   it is returned as is.
        /// - If the /AcroForm key does not exist or is not a dictionary, a new
        ///   dictionary is created, stored as the /AcroForm entry in the document root,
        ///   and then returned.
        ///
        /// @return A QPDFObjectHandle representing the /AcroForm dictionary.
        QPDFObjectHandle getOrCreateAcroForm();

        /// Adjusts inherited field properties for an AcroForm field object.
        ///
        /// This method ensures that the `/DA` (default appearance) and `/Q` (quadding) keys
        /// of the specified field object are overridden if necessary, based on the provided
        /// parameters. The overriding is performed only if the respective `override_da` or
        /// `override_q` flags are set to true, and when the original object's values differ from
        /// the provided defaults. No changes are made to fields that have explicit values for `/DA`
        /// or `/Q`.
        ///
        /// The function is primarily used for adjusting inherited form field properties in cases
        /// where the document structure or inherited values have changed (e.g., when working with
        /// fields in a PDF document).
        ///
        /// @param obj The `QPDFObjectHandle` instance representing the form field object to be
        ///            adjusted.
        /// @param override_da A boolean flag indicating whether to override the `/DA` key.
        /// @param from_default_da The default appearance string to apply if overriding the `/DA`
        ///                        key.
        /// @param override_q A boolean flag indicating whether to override the `/Q` key.
        /// @param from_default_q The default quadding value (alignment) to apply if overriding the
        ///                       `/Q` key.
        void adjustInheritedFields(
            QPDFObjectHandle obj,
            bool override_da,
            std::string const& from_default_da,
            bool override_q,
            int from_default_q);

        /// Adjusts the default appearances (/DA) of an AcroForm field object.
        ///
        /// This method ensures that form fields copied from another PDF document
        /// have their default appearances resource references updated to correctly
        /// point to the appropriate resources in the current document's resource
        /// dictionary (/DR). It resolves name conflicts between the dictionaries
        /// of the source and destination documents by using a mapping provided in
        /// `dr_map`.
        ///
        /// The method parses the /DA string, processes its resource references,
        /// and regenerates the /DA with updated references.
        ///
        /// @param obj The AcroForm field object whose /DA is being adjusted.
        /// @param dr_map A mapping between resource names in the source document's
        ///        resource dictionary and their corresponding names in the current
        ///        document's resource dictionary.
        void adjustDefaultAppearances(
            QPDFObjectHandle obj,
            std::map<std::string, std::map<std::string, std::string>> const& dr_map);

        /// Modifies the appearance stream of an AcroForm field to ensure its resources
        /// align with the resource dictionary and appearance settings. This method
        /// ensures proper resource handling to avoid any conflicts when regenerating
        /// the appearance stream.
        ///
        /// Adjustments include:
        /// - Creating a private resource dictionary for the stream if not already present.
        /// - Merging top-level resource keys into the stream's resource dictionary.
        /// - Resolving naming conflicts between existing and remapped resource keys.
        /// - Removing empty sub-dictionaries from the resource dictionary.
        /// - Attaching a token filter to rewrite resource references in the stream content.
        ///
        /// If conflicts between keys are encountered or the stream cannot be parsed successfully,
        /// appropriate warnings will be generated instead of halting execution.
        ///
        /// @param stream The QPDFObjectHandle representation of the PDF appearance stream to be
        ///               adjusted.
        /// @param dr_map A mapping of resource types and their corresponding name remappings
        ///               used for resolving resource conflicts and regenerating appearances.
        void adjustAppearanceStream(
            QPDFObjectHandle stream,
            std::map<std::string, std::map<std::string, std::string>> dr_map);

        std::map<QPDFObjGen, FieldData> fields_;
        std::map<QPDFObjGen, QPDFFormFieldObjectHelper> annotation_to_field_;
        std::map<std::string, std::set<QPDFObjGen>> name_to_fields_;
        std::set<QPDFObjGen> bad_fields_;
        bool cache_valid_{false};

    }; // class Acroform

    /// @class FormNode
    /// @brief Represents a node in the interactive forms tree of a PDF document.
    ///
    /// This class models nodes that may be either form field dictionaries or widget annotation
    /// dictionaries, as defined in the PDF specification (sections 12.7 and 12.5.6.19).
    ///
    /// For a detailed description of the attributes that this class can expose, refer to the
    /// corresponding tables in the PDF 2.0 (Table 226) or PDF 1.7 (Table 220) specifications.
    class FormNode: public qpdf::BaseDictionary
    {
      public:
        FormNode() = default;
        FormNode(FormNode const&) = default;
        FormNode& operator=(FormNode const&) = default;
        FormNode(FormNode&&) = default;
        FormNode& operator=(FormNode&&) = default;
        ~FormNode() = default;

        FormNode(QPDFObjectHandle const& oh) :
            BaseDictionary(oh)
        {
        }

        FormNode(QPDFObjectHandle&& oh) :
            BaseDictionary(std::move(oh))
        {
        }

        /// Retrieves the /Parent form field of the current field.
        ///
        /// This function accesses the parent field in the hierarchical structure of form fields, if
        /// it exists. The parent is determined based on the /Parent attribute in the field
        /// dictionary.
        ///
        /// @return A FormNode object representing the parent field. If the current field has no
        ///         parent, an empty FormNode object is returned.
        FormNode
        Parent()
        {
            return {get("/Parent")};
        }

        /// @brief Returns the top-level field associated with the current field.
        ///
        /// The function traverses the hierarchy of parent fields to identify the highest-level
        /// field in the tree. Typically, this will be the current field itself unless it has a
        /// parent field. Optionally, it can indicate whether the top-level field is different from
        /// the current field.
        ///
        /// @param is_different A pointer to a boolean that, if provided, will be set to true if the
        ///        top-level field differs from the current field; otherwise, it will be set to
        ///        false.
        ///
        /// @return The top-level field in the form field hierarchy.
        FormNode root_field(bool* is_different = nullptr);

        /// @brief Retrieves the inherited value of the specified attribute.
        ///
        /// @param name The name of the attribute to retrieve.
        /// @param acroform If true, checks the document's /AcroForm dictionary for the attribute
        ///                 if it is not found in the field hierarchy.
        ///
        /// @return A constant reference to the QPDFObjectHandle representing the value of the
        ///         specified attribute, if found. If the attribute is not found in the field
        ///         hierarchy or the /AcroForm dictionary (when `acroform` is true), returns a
        ///         reference to a static null object handle.
        QPDFObjectHandle const& inherited(std::string const& name, bool acroform = false) const;

        /// @brief Retrieves the value of a specified field, accounting for inheritance through the
        ///        hierarchy of ancestor nodes in the form field tree.
        ///
        /// This function attempts to retrieve the value of the specified field. If the `inherit`
        /// parameter is set to `true` and the field value is not found at the current level, the
        /// method traverses up the parent hierarchy to find the value. The traversal stops when a
        /// value is found, when the root node is reached, or when a loop detection mechanism
        /// prevents further traversal.
        ///
        /// @tparam T The return type of the field value.
        /// @param name The name of the field to retrieve the value for.
        /// @param inherit If set to `true`, the function will attempt to retrieve the value by
        ///        inheritance from the parent hierarchy of the form field. Defaults to `true`.
        /// @return Returns the field's value if found; otherwise, returns a default-constructed
        ///         value of type `T`.
        template <class T>
        T
        inheritable_value(std::string const& name, bool inherit = true, bool acroform = false) const
        {
            if (auto& v = get(name)) {
                return {v};
            }
            return {inherit ? inherited(name, acroform) : null_oh};
        }

        /// @brief Retrieves an inherited field string attribute as a string.
        ///
        /// @param name The name of the field for which the value is to be retrieved.
        /// @return The inherited field value as a UTF-8 encoded string, or an empty string if the
        ///         value does not exist or is not of String type.
        std::string inheritable_string(std::string const& name) const;

        /// @brief Retrieves the field type (/FT attribute).
        ///
        /// @param inherit If set to `true`, the function will attempt to retrieve the value by
        ///        inheritance from the parent hierarchy of the form field. Defaults to `true`.
        /// @return Returns the field type if found; otherwise, returns a default-constructed
        ///         `Name`.
        Name
        FT(bool inherit = true) const
        {
            return inheritable_value<Name>("/FT");
        }

        /// @brief Retrieves the partial field name (/T attribute).
        ///
        /// @return Returns the partial field name if found; otherwise, returns a
        ///         default-constructed `String`.
        String
        T() const
        {
            return {get("/T")};
        }

        /// @brief Retrieves the alternative name (/TU attribute).
        ///
        /// @return Returns the alternative name if found; otherwise, returns a default-constructed
        ///         `String`.
        String
        TU() const
        {
            return {get("/TU")};
        }

        /// @brief Retrieves the mapping name (/TM attribute).
        ///
        /// @return Returns the mapping name if found; otherwise, returns a default-constructed
        ///         `String`.
        String
        TM() const
        {
            return {get("/TM")};
        }

        /// @brief Retrieves the fully qualified name of the form field.
        ///
        /// This method constructs the fully qualified name of the form field by traversing through
        /// its parent hierarchy. The fully qualified name is constructed by concatenating the /T
        /// (field name) attribute of each parent node with periods as separators, starting from the
        /// root of the hierarchy.
        ///
        /// If the field has no parent hierarchy, the result will simply be the /T attribute of the
        /// current field. In cases of potential circular references, loop detection is applied.
        ///
        /// @return A string representing the fully qualified name of the field.
        std::string fully_qualified_name() const;

        /// @brief Retrieves the partial name (/T attribute) of the form field.
        ///
        /// This method returns the value of the field's /T attribute, which is the partial name
        /// used to identify the field within its parent hierarchy. If the attribute is not set, an
        /// empty string is returned.
        ///
        /// @return A string representing the partial name of the field in UTF-8 encoding, or an
        ///         empty string if the /T attribute is not present.
        std::string partial_name() const;

        /// @brief Retrieves the alternative name for the form field.
        ///
        /// This method attempts to return the alternative name (/TU) of the form field, which is
        /// the field name intended to be presented, to users as a UTF-8 string, if it exists. If
        /// the alternative name is not present, the method falls back to the fully qualified name
        /// of the form field.
        ///
        /// @return The alternative name of the form field as a string, or the
        /// fully qualified name if the alternative name is unavailable.
        std::string alternative_name() const;

        /// @brief Retrieves the mapping field name (/TM) for the form field.
        ///
        /// If the mapping name (/TM) is present, it is returned as a UTF-8 string. If not, it falls
        /// back to the 'alternative name', which is obtained using the `alternative_name()` method.
        ///
        /// @return The mapping field name (/TM) as a UTF-8 string or the alternative name if the
        ///         mapping name is absent.
        std::string mapping_name() const;

        /// @brief Retrieves the field value (`/V` attribute) of a specified field, accounting for
        ///        inheritance through thehierarchy of ancestor nodes in the form field tree.
        ///
        /// This function attempts to retrieve the `/V` attribute. If the `inherit`
        /// parameter is set to `true` and the `/V` is not found at the current level, the
        /// method traverses up the parent hierarchy to find the value. The traversal stops when
        /// `/V` is found, when the root node is reached, or when a loop detection mechanism
        /// prevents further traversal.
        ///
        /// @tparam T The return type.
        /// @param inherit If set to `true`, the function will attempt to retrieve `/V` by
        ///        inheritance from the parent hierarchy of the form field. Defaults to `true`.
        /// @return Returns the field's value if found; otherwise, returns a default-constructed
        /// value of type `T`.
        template <class T>
        T
        V(bool inherit = true) const
        {
            return inheritable_value<T>("/V", inherit);
        }

        /// @brief Retrieves the field value (`/V` attribute) of a specified field, accounting for
        ///        inheritance through the hierarchy of ancestor nodes in the form field tree.
        ///
        /// This function attempts to retrieve the `/V` attribute. If the `inherit`
        /// parameter is set to `true` and the `/V` is not found at the current level, the
        /// method traverses up the parent hierarchy to find the value. The traversal stops when
        /// `/V` is found, when the root node is reached, or when a loop detection mechanism
        /// prevents further traversal.
        ///
        /// @param inherit If set to `true`, the function will attempt to retrieve `/V` by
        ///        inheritance from the parent hierarchy of the form field. Defaults to `true`.
        /// @return Returns the field's value if found; otherwise, returns a default-constructed
        /// object handle.
        QPDFObjectHandle const&
        V(bool inherit = true) const
        {
            if (auto& v = get("/V")) {
                return v;
            }
            return {inherit ? inherited("/V") : null_oh};
        }

        /// @brief Retrieves the field value `/V` attribute of the form field, considering
        ///        inheritance, if the value is  a String.
        ///
        /// This function extracts the value of the form field, accounting for potential inheritance
        /// through the form hierarchy. It returns the value if it is a String, and an empty string
        /// otherwise.
        ///
        /// @return A string containing the actual or inherited `/V` attribute of the form field, or
        ///         an empty string if the value is not present or not a String.
        std::string value() const;

        /// @brief Retrieves the field  default value (`/DV` attribute) of a specified field,
        ///        accounting for inheritance through the hierarchy of ancestor nodes in the form
        ///        field tree.
        ///
        /// This function attempts to retrieve the `/DV` attribute. If the `inherit` parameter is
        /// set to `true` and the `/DV` is not found at the current level, the method traverses up
        /// the parent hierarchy to find the value. The traversal stops when
        /// `/DV` is found, when the root node is reached, or when a loop detection mechanism
        /// prevents further traversal.
        ///
        /// @tparam T The return type.
        /// @param inherit If set to `true`, the function will attempt to retrieve `/DV` by
        ///        inheritance from the parent hierarchy of the form field. Defaults to `true`.
        /// @return Returns the field's default value if found; otherwise, returns a
        ///         default-constructed value of type `T`.
        QPDFObjectHandle const&
        DV(bool inherit = true) const
        {
            if (auto& v = get("/DV")) {
                return v;
            }
            return {inherit ? inherited("/DV") : null_oh};
        }

        /// @brief Retrieves the default value `/DV` attribute of the form field, considering
        ///        inheritance, if the default value is  a String.
        ///
        /// This function extracts the default value of the form field, accounting for potential
        /// inheritance through the form hierarchy. It returns the value if it is a String, and an
        /// empty string otherwise.
        ///
        /// @return A string containing the actual or inherited `/V` attribute of the form field, or
        ///         an empty string if the value is not present or not a String.
        std::string default_value() const;

        /// @brief Returns the default appearance string for the form field, considering inheritance
        ///        from the field tree hierarchy and the document's /AcroForm dictionary.
        ///
        /// This method retrieves the field's /DA (default appearance) attribute. If the attribute
        /// is not directly available, it checks the parent fields in the hierarchy for an inherited
        /// value. If no value is found in the field hierarchy, it attempts to retrieve the /DA
        /// attribute from the document's /AcroForm dictionary. The method returns an empty string
        /// if no default appearance string is available or applicable.
        ///
        /// @return A string representing the default appearance, or an empty string if
        ///         no value is found.
        std::string default_appearance() const;

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
        /// @brief Retrieves an entry from the document's /AcroForm dictionary using the specified
        /// name.
        ///
        /// The method accesses the AcroForm dictionary within the root object of the PDF document.
        /// If the the AcroForm dictionary contains the given field name, it retrieves the
        /// corresponding entry. Otherwise, it returns a default-constructed object handle.
        ///
        /// @param name The name of the form field to retrieve.
        /// @return A object handle corresponding to the specified name within the AcroForm
        /// dictionary.
        QPDFObjectHandle const&
        from_AcroForm(std::string const& name) const
        {
            return {qpdf() ? qpdf()->getRoot()["/AcroForm"][name] : null_oh};
        }

        void setRadioButtonValue(QPDFObjectHandle name);
        void setCheckBoxValue(bool value);
        void generateTextAppearance(QPDFAnnotationObjectHelper&);
        QPDFObjectHandle
        getFontFromResource(QPDFObjectHandle resources, std::string const& font_name);

        static const QPDFObjectHandle null_oh;
    }; // class FormNode
} // namespace qpdf::impl

#endif // ACRO_FORM_HH

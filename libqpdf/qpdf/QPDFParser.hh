#ifndef QPDFPARSER_HH
#define QPDFPARSER_HH

#include <qpdf/InputSource_private.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFTokenizer_private.hh>
#include <qpdf/global_private.hh>

#include <memory>
#include <string>

using namespace qpdf;
using namespace qpdf::global;

namespace qpdf::impl
{
    /// @class  Parser
    /// @brief  Internal parser for PDF objects and content streams.
    /// @par
    ///         The Parser class provides static methods for parsing PDF objects from input sources.
    ///         It handles tokenization, error recovery, and object construction with proper offset
    ///         tracking and description for error reporting.
    class Parser
    {
      public:
        /// @brief Exception thrown when parser encounters an unrecoverable error.
        class Error: public std::exception
        {
          public:
            Error() = default;
            virtual ~Error() noexcept = default;
        };

        /// @brief Parse a PDF object from an input source.
        /// @param input The input source to read from.
        /// @param object_description Description of the object for error messages.
        /// @param context The QPDF context, or nullptr if parsing standalone.
        /// @return The parsed QPDFObjectHandle, or null if parsing fails.
        static QPDFObjectHandle
        parse(InputSource& input, std::string const& object_description, QPDF* context);

        /// @brief Parse a content stream from an input source.
        /// @param input The input source to read from.
        /// @param sp_description Shared pointer to object description.
        /// @param tokenizer The tokenizer to use for parsing.
        /// @param context The QPDF context.
        /// @return The parsed QPDFObjectHandle, or uninitialized handle on EOF.
        static QPDFObjectHandle parse_content(
            InputSource& input,
            std::shared_ptr<QPDFObject::Description> sp_description,
            qpdf::Tokenizer& tokenizer,
            QPDF* context);

        /// @brief Parse a PDF object (interface for deprecated QPDFObjectHandle::parse).
        /// @param input The input source to read from.
        /// @param object_description Description of the object for error messages.
        /// @param tokenizer The tokenizer to use for parsing.
        /// @param empty Output parameter indicating if object was empty.
        /// @param decrypter String decrypter for encrypted strings, or nullptr.
        /// @param context The QPDF context, or nullptr if parsing standalone.
        /// @return The parsed QPDFObjectHandle.
        static QPDFObjectHandle parse(
            InputSource& input,
            std::string const& object_description,
            QPDFTokenizer& tokenizer,
            bool& empty,
            QPDFObjectHandle::StringDecrypter* decrypter,
            QPDF* context);

        /// @brief Parse a PDF object for use by QPDF.
        /// @param input The input source to read from.
        /// @param object_description Description of the object for error messages.
        /// @param tokenizer The tokenizer to use for parsing.
        /// @param decrypter String decrypter for encrypted strings, or nullptr.
        /// @param context The QPDF context.
        /// @param sanity_checks Enable additional sanity checks during parsing.
        /// @return The parsed QPDFObjectHandle.
        static QPDFObjectHandle parse(
            InputSource& input,
            std::string const& object_description,
            qpdf::Tokenizer& tokenizer,
            QPDFObjectHandle::StringDecrypter* decrypter,
            QPDF& context,
            bool sanity_checks);

        /// @brief Parse an object from an object stream.
        /// @param input The offset buffer containing the object data.
        /// @param stream_id The object stream number.
        /// @param obj_id The object ID within the stream.
        /// @param tokenizer The tokenizer to use for parsing.
        /// @param context The QPDF context.
        /// @return The parsed QPDFObjectHandle.
        static QPDFObjectHandle parse(
            qpdf::is::OffsetBuffer& input,
            int stream_id,
            int obj_id,
            qpdf::Tokenizer& tokenizer,
            QPDF& context);

        /// @brief Create a description for a parsed object.
        /// @param input_name The name of the input source.
        /// @param object_description Description of the object being parsed.
        /// @return Shared pointer to object description with offset placeholder.
        static std::shared_ptr<QPDFObject::Description>
        make_description(std::string const& input_name, std::string const& object_description)
        {
            using namespace std::literals;
            return std::make_shared<QPDFObject::Description>(
                input_name + ", " + object_description + " at offset $PO");
        }

      private:
        /// @brief Construct a parser instance.
        /// @param input The input source to read from.
        /// @param sp_description Shared pointer to object description.
        /// @param object_description Description string for error messages.
        /// @param tokenizer The tokenizer to use for parsing.
        /// @param decrypter String decrypter for encrypted content.
        /// @param context The QPDF context.
        /// @param parse_pdf Whether parsing PDF objects (vs content streams).
        /// @param stream_id Object stream ID for object stream parsing.
        /// @param obj_id Object ID within object stream.
        /// @param sanity_checks Enable additional sanity checks.
        Parser(
            InputSource& input,
            std::shared_ptr<QPDFObject::Description> sp_description,
            std::string const& object_description,
            qpdf::Tokenizer& tokenizer,
            QPDFObjectHandle::StringDecrypter* decrypter,
            QPDF* context,
            bool parse_pdf,
            int stream_id = 0,
            int obj_id = 0,
            bool sanity_checks = false) :
            input_(input),
            object_description_(object_description),
            tokenizer_(tokenizer),
            decrypter_(decrypter),
            context_(context),
            description_(std::move(sp_description)),
            parse_pdf_(parse_pdf),
            stream_id_(stream_id),
            obj_id_(obj_id),
            sanity_checks_(sanity_checks)
        {
        }

        /// @brief Parser state enumeration.
        /// @note state <= st_dictionary_value indicates we're in a dictionary context.
        enum parser_state_e { st_dictionary_key, st_dictionary_value, st_array };

        /// @brief Stack frame for tracking nested arrays and dictionaries.
        struct StackFrame
        {
            StackFrame(InputSource& input, parser_state_e state) :
                state(state),
                offset(input.tell())
            {
            }

            std::vector<QPDFObjectHandle> olist;          ///< Object list for arrays/dict values
            std::map<std::string, QPDFObjectHandle> dict; ///< Dictionary entries
            parser_state_e state;                         ///< Current parser state
            std::string key;                              ///< Current dictionary key
            qpdf_offset_t offset;                         ///< Offset of container start
            std::string contents_string;                  ///< For /Contents field in signatures
            qpdf_offset_t contents_offset{-1};            ///< Offset of /Contents value
            int null_count{0};                            ///< Count of null values in container
        };

        /// @brief Parse an object, handling exceptions and returning null on error.
        /// @param content_stream True if parsing a content stream.
        /// @return The parsed object handle, or null/uninitialized on error.
        QPDFObjectHandle parse(bool content_stream = false);

        /// @brief Parse the first token and dispatch to appropriate handler.
        /// @param content_stream True if parsing a content stream.
        /// @return The parsed object handle.
        QPDFObjectHandle parse_first(bool content_stream);

        /// @brief Parse the remainder of a composite object (array/dict/reference).
        /// @param content_stream True if parsing a content stream.
        /// @return The completed object handle.
        QPDFObjectHandle parse_remainder(bool content_stream);

        /// @brief Add an object to the current container.
        /// @param obj The object to add.
        void add(std::shared_ptr<QPDFObject>&& obj);

        /// @brief Add a null object to the current container.
        void add_null();

        /// @brief Add a null with a warning message.
        /// @param msg Warning message describing the error.
        void add_bad_null(std::string const& msg);

        /// @brief Add a buffered integer from int_buffer_.
        /// @param count Buffer index (1 or 2) to read from.
        void add_int(int count);

        /// @brief Create and add a scalar object to the current container.
        /// @tparam T The scalar object type (e.g., QPDF_Integer, QPDF_String).
        /// @tparam Args Constructor argument types.
        /// @param args Arguments to forward to the object constructor.
        template <typename T, typename... Args>
        void add_scalar(Args&&... args);

        /// @brief Check if too many bad tokens have been encountered and throw if so.
        void check_too_many_bad_tokens();

        /// @brief Issue a warning about a duplicate dictionary key.
        void warn_duplicate_key();

        /// @brief Fix dictionaries with missing keys by generating fake keys.
        void fix_missing_keys();

        /// @brief Report a limits error and throw.
        /// @param limit The limit identifier.
        /// @param msg Error message.
        [[noreturn]] void limits_error(std::string const& limit, std::string const& msg);

        /// @brief Issue a warning at a specific offset.
        /// @param offset File offset for the warning.
        /// @param msg Warning message.
        void warn(qpdf_offset_t offset, std::string const& msg) const;

        /// @brief Issue a warning at the current offset.
        /// @param msg Warning message.
        void warn(std::string const& msg) const;

        /// @brief Issue a warning from a QPDFExc exception.
        /// @param e The exception to report.
        void warn(QPDFExc const& e) const;

        /// @brief Create a scalar object with description and parsed offset.
        /// @tparam T The scalar object type.
        /// @tparam Args Constructor argument types.
        /// @param args Arguments to forward to the object constructor.
        /// @return Object handle with description and offset set.
        /// @note The offset includes any leading whitespace.
        template <typename T, typename... Args>
        QPDFObjectHandle with_description(Args&&... args);

        /// @brief Set the description and offset on an existing object.
        /// @param obj The object to update.
        /// @param parsed_offset The file offset where the object was parsed.
        void set_description(std::shared_ptr<QPDFObject>& obj, qpdf_offset_t parsed_offset);

        // Core parsing state
        InputSource& input_;                           ///< Input source to read from
        std::string const& object_description_;        ///< Description for error messages
        qpdf::Tokenizer& tokenizer_;                   ///< Tokenizer for lexical analysis
        QPDFObjectHandle::StringDecrypter* decrypter_; ///< Decrypter for encrypted strings
        QPDF* context_;                                ///< QPDF context for object resolution
        std::shared_ptr<QPDFObject::Description> description_; ///< Shared description for objects
        bool parse_pdf_{false};     ///< True if parsing PDF objects vs content streams
        int stream_id_{0};          ///< Object stream ID (for object stream parsing)
        int obj_id_{0};             ///< Object ID within object stream
        bool sanity_checks_{false}; ///< Enable additional validation checks

        // Composite object parsing state
        std::vector<StackFrame> stack_; ///< Stack of nested containers
        StackFrame* frame_{nullptr};    ///< Current stack frame pointer

        // Error tracking state
        /// Number of recent bad tokens. Always > 0 after first bad token encountered.
        int bad_count_{0};
        /// Number of bad tokens remaining before giving up.
        uint32_t max_bad_count_{Limits::parser_max_errors()};
        /// Number of good tokens since last bad token. Irrelevant if bad_count == 0.
        int good_count_{0};

        // Token buffering state
        /// Start offset of current object, including any leading whitespace.
        qpdf_offset_t start_{0};
        /// Number of successive integer tokens (for indirect reference detection).
        int int_count_{0};
        /// Buffer for up to 2 integer tokens.
        long long int_buffer_[2]{0, 0};
        /// Offsets corresponding to buffered integers.
        qpdf_offset_t last_offset_buffer_[2]{0, 0};

        /// True if object was empty (endobj without content).
        bool empty_{false};
    };
} // namespace qpdf::impl

#endif // QPDFPARSER_HH

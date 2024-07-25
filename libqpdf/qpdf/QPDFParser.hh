#ifndef QPDFPARSER_HH
#define QPDFPARSER_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFValue.hh>

#include <memory>
#include <string>

class QPDFParser
{
  public:
    QPDFParser() = delete;
    QPDFParser(
        std::shared_ptr<InputSource> input,
        std::string const& object_description,
        QPDFTokenizer& tokenizer,
        QPDFObjectHandle::StringDecrypter* decrypter,
        QPDF* context,
        bool parse_pdf) :
        input(input),
        object_description(object_description),
        tokenizer(tokenizer),
        decrypter(decrypter),
        context(context),
        description(std::make_shared<QPDFValue::Description>(
            std::string(input->getName() + ", " + object_description + " at offset $PO")))
    {
    }
    virtual ~QPDFParser() = default;

    QPDFObjectHandle parse(bool& empty, bool content_stream);

  private:
    // Parser state.  Note:
    // state <= st_dictionary_value == (state = st_dictionary_key || state = st_dictionary_value)
    enum parser_state_e { st_dictionary_key, st_dictionary_value, st_array };

    struct StackFrame
    {
        StackFrame(std::shared_ptr<InputSource> const& input, parser_state_e state) :
            state(state),
            offset(input->tell())
        {
        }

        std::vector<std::shared_ptr<QPDFObject>> olist;
        std::map<std::string, QPDFObjectHandle> dict;
        parser_state_e state;
        std::string key;
        qpdf_offset_t offset;
        std::string contents_string;
        qpdf_offset_t contents_offset{-1};
        int null_count{0};
    };

    QPDFObjectHandle parseRemainder(bool content_stream);
    void add(std::shared_ptr<QPDFObject>&& obj);
    void addNull();
    void addInt(int count);
    template <typename T, typename... Args>
    void addScalar(Args&&... args);
    bool tooManyBadTokens();
    void warnDuplicateKey();
    void fixMissingKeys();
    void warn(qpdf_offset_t offset, std::string const& msg) const;
    void warn(std::string const& msg) const;
    void warn(QPDFExc const&) const;
    template <typename T, typename... Args>
    // Create a new scalar object complete with parsed offset and description.
    // NB the offset includes any leading whitespace.
    QPDFObjectHandle withDescription(Args&&... args);
    void setDescription(std::shared_ptr<QPDFObject>& obj, qpdf_offset_t parsed_offset);
    std::shared_ptr<InputSource> input;
    std::string const& object_description;
    QPDFTokenizer& tokenizer;
    QPDFObjectHandle::StringDecrypter* decrypter;
    QPDF* context;
    std::shared_ptr<QPDFValue::Description> description;
    std::vector<StackFrame> stack;
    StackFrame* frame;
    // Number of recent bad tokens.
    int bad_count = 0;
    // Number of good tokens since last bad token. Irrelevant if bad_count == 0.
    int good_count = 0;
    // Start offset including any leading whitespace.
    qpdf_offset_t start;
    // Number of successive integer tokens.
    int int_count = 0;
    long long int_buffer[2]{0, 0};
    qpdf_offset_t last_offset_buffer[2]{0, 0};
};

#endif // QPDFPARSER_HH

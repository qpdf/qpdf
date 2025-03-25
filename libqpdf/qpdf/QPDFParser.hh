#ifndef QPDFPARSER_HH
#define QPDFPARSER_HH

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFTokenizer_private.hh>

#include <memory>
#include <string>

class QPDFParser
{
  public:
    static QPDFObjectHandle
    parse(InputSource& input, std::string const& object_description, QPDF* context);

    static QPDFObjectHandle parse_content(
        InputSource& input,
        std::shared_ptr<QPDFObject::Description> sp_description,
        qpdf::Tokenizer& tokenizer,
        QPDF* context);

    // For use by deprecated QPDFObjectHandle::parse.
    static QPDFObjectHandle parse(
        InputSource& input,
        std::string const& object_description,
        QPDFTokenizer& tokenizer,
        bool& empty,
        QPDFObjectHandle::StringDecrypter* decrypter,
        QPDF* context);

    // For use by QPDF. Return parsed object and whether it is empty.
    static std::pair<QPDFObjectHandle, bool> parse(
        InputSource& input,
        std::string const& object_description,
        qpdf::Tokenizer& tokenizer,
        QPDFObjectHandle::StringDecrypter* decrypter,
        QPDF& context);

    static std::pair<QPDFObjectHandle, bool> parse(
        BufferInputSource& input,
        int stream_id,
        int obj_id,
        qpdf::Tokenizer& tokenizer,
        QPDF& context);

    static std::shared_ptr<QPDFObject::Description>
    make_description(std::string const& input_name, std::string const& object_description)
    {
        using namespace std::literals;
        return std::make_shared<QPDFObject::Description>(
            input_name + ", " + object_description + " at offset $PO");
    }

  private:
    QPDFParser(
        InputSource& input,
        std::shared_ptr<QPDFObject::Description> sp_description,
        std::string const& object_description,
        qpdf::Tokenizer& tokenizer,
        QPDFObjectHandle::StringDecrypter* decrypter,
        QPDF* context,
        bool parse_pdf,
        int stream_id = 0,
        int obj_id = 0) :
        input(input),
        object_description(object_description),
        tokenizer(tokenizer),
        decrypter(decrypter),
        context(context),
        description(std::move(sp_description)),
        parse_pdf(parse_pdf),
        stream_id(stream_id),
        obj_id(obj_id)
    {
    }

    // Parser state.  Note:
    // state <= st_dictionary_value == (state = st_dictionary_key || state = st_dictionary_value)
    enum parser_state_e { st_dictionary_key, st_dictionary_value, st_array };

    struct StackFrame
    {
        StackFrame(InputSource& input, parser_state_e state) :
            state(state),
            offset(input.tell())
        {
        }

        std::vector<QPDFObjectHandle> olist;
        std::map<std::string, QPDFObjectHandle> dict;
        parser_state_e state;
        std::string key;
        qpdf_offset_t offset;
        std::string contents_string;
        qpdf_offset_t contents_offset{-1};
        int null_count{0};
    };

    QPDFObjectHandle parse(bool& empty, bool content_stream);
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
    InputSource& input;
    std::string const& object_description;
    qpdf::Tokenizer& tokenizer;
    QPDFObjectHandle::StringDecrypter* decrypter;
    QPDF* context;
    std::shared_ptr<QPDFObject::Description> description;
    bool parse_pdf{false};
    int stream_id{0};
    int obj_id{0};

    std::vector<StackFrame> stack;
    StackFrame* frame{nullptr};
    // Number of recent bad tokens. This will always be > 0 once a bad token has been encountered as
    // it only gets incremented or reset when a bad token is encountered.
    int bad_count{0};
    // Number of bad tokens (remaining) before giving up.
    int max_bad_count{15};
    // Number of good tokens since last bad token. Irrelevant if bad_count == 0.
    int good_count{0};
    // Start offset including any leading whitespace.
    qpdf_offset_t start{0};
    // Number of successive integer tokens.
    int int_count{0};
    long long int_buffer[2]{0, 0};
    qpdf_offset_t last_offset_buffer[2]{0, 0};
};

#endif // QPDFPARSER_HH

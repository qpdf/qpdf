#ifndef QPDFPARSER_HH
#define QPDFPARSER_HH

#include <qpdf/QPDFObjectHandle.hh>

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
        QPDF* context) :
        input(input),
        object_description(object_description),
        tokenizer(tokenizer),
        decrypter(decrypter),
        context(context)
    {
    }
    virtual ~QPDFParser() = default;

    QPDFObjectHandle parse(bool& empty, bool content_stream);

  private:
    enum parser_state_e {
        st_top,
        st_start,
        st_stop,
        st_eof,
        st_dictionary,
        st_array
    };

    void warn(qpdf_offset_t offset, std::string const& msg) const;
    void warn(std::string const& msg) const;
    static void warn(QPDF*, QPDFExc const&);
    void setParsedOffset(qpdf_offset_t offset);

    std::shared_ptr<InputSource> input;
    std::string const& object_description;
    QPDFTokenizer& tokenizer;
    QPDFObjectHandle::StringDecrypter* decrypter;
    QPDF* context;
};

#endif // QPDFPARSER_HH

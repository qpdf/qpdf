#include <qpdf/BufferInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFTokenizer.hh>
#include <qpdf/QUtil.hh>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami
              << " [-maxlen len | -no-ignorable] filename" << std::endl;
    exit(2);
}

class Finder: public InputSource::Finder
{
  public:
    Finder(std::shared_ptr<InputSource> is, std::string const& str) :
        is(is),
        str(str)
    {
    }
    virtual ~Finder() = default;
    virtual bool check();

  private:
    std::shared_ptr<InputSource> is;
    std::string str;
};

bool
Finder::check()
{
    QPDFTokenizer tokenizer;
    QPDFTokenizer::Token t = tokenizer.readToken(is, "finder", true);
    qpdf_offset_t offset = this->is->tell();
    bool result = (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, str));
    this->is->seek(offset - QIntC::to_offset(this->str.length()), SEEK_SET);
    return result;
}

static char const*
tokenTypeName(QPDFTokenizer::token_type_e ttype)
{
    // Do this is a case statement instead of a lookup so the compiler
    // will warn if we miss any.
    switch (ttype) {
    case QPDFTokenizer::tt_bad:
        return "bad";
    case QPDFTokenizer::tt_array_close:
        return "array_close";
    case QPDFTokenizer::tt_array_open:
        return "array_open";
    case QPDFTokenizer::tt_brace_close:
        return "brace_close";
    case QPDFTokenizer::tt_brace_open:
        return "brace_open";
    case QPDFTokenizer::tt_dict_close:
        return "dict_close";
    case QPDFTokenizer::tt_dict_open:
        return "dict_open";
    case QPDFTokenizer::tt_integer:
        return "integer";
    case QPDFTokenizer::tt_name:
        return "name";
    case QPDFTokenizer::tt_real:
        return "real";
    case QPDFTokenizer::tt_string:
        return "string";
    case QPDFTokenizer::tt_null:
        return "null";
    case QPDFTokenizer::tt_bool:
        return "bool";
    case QPDFTokenizer::tt_word:
        return "word";
    case QPDFTokenizer::tt_eof:
        return "eof";
    case QPDFTokenizer::tt_space:
        return "space";
    case QPDFTokenizer::tt_comment:
        return "comment";
    case QPDFTokenizer::tt_inline_image:
        return "inline-image";
    }
    return nullptr;
}

static std::string
sanitize(std::string const& value)
{
    std::string result;
    for (auto const& iter: value) {
        if ((iter >= 32) && (iter <= 126)) {
            result.append(1, iter);
        } else {
            result += "\\x" +
                QUtil::int_to_string_base(
                          static_cast<unsigned char>(iter), 16, 2);
        }
    }
    return result;
}

static void
try_skipping(
    QPDFTokenizer& tokenizer,
    std::shared_ptr<InputSource> is,
    size_t max_len,
    char const* what,
    Finder& f)
{
    std::cout << "skipping to " << what << std::endl;
    qpdf_offset_t offset = is->tell();
    if (!is->findFirst(what, offset, 0, f)) {
        std::cout << what << " not found" << std::endl;
        is->seek(offset, SEEK_SET);
    }
}

static void
dump_tokens(
    std::shared_ptr<InputSource> is,
    std::string const& label,
    size_t max_len,
    bool include_ignorable,
    bool skip_streams,
    bool skip_inline_images)
{
    Finder f1(is, "endstream");
    std::cout << "--- BEGIN " << label << " ---" << std::endl;
    bool done = false;
    QPDFTokenizer tokenizer;
    tokenizer.allowEOF();
    if (include_ignorable) {
        tokenizer.includeIgnorable();
    }
    qpdf_offset_t inline_image_offset = 0;
    while (!done) {
        QPDFTokenizer::Token token = tokenizer.readToken(
            is, "test", true, inline_image_offset ? 0 : max_len);
        if (inline_image_offset && (token.getType() == QPDFTokenizer::tt_bad)) {
            std::cout << "EI not found; resuming normal scanning" << std::endl;
            is->seek(inline_image_offset, SEEK_SET);
            inline_image_offset = 0;
            continue;
        }
        inline_image_offset = 0;

        qpdf_offset_t offset = is->getLastOffset();
        std::cout << offset << ": " << tokenTypeName(token.getType());
        if (token.getType() != QPDFTokenizer::tt_eof) {
            std::cout << ": " << sanitize(token.getValue());
            if (token.getValue() != token.getRawValue()) {
                std::cout << " (raw: " << sanitize(token.getRawValue()) << ")";
            }
        }
        if (!token.getErrorMessage().empty()) {
            std::cout << " (" << token.getErrorMessage() << ")";
        }
        std::cout << std::endl;
        if (skip_streams &&
            (token == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "stream"))) {
            try_skipping(tokenizer, is, max_len, "endstream", f1);
        } else if (
            skip_inline_images &&
            (token == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "ID"))) {
            char ch;
            is->read(&ch, 1);
            tokenizer.expectInlineImage(is);
            inline_image_offset = is->tell();
        } else if (token.getType() == QPDFTokenizer::tt_eof) {
            done = true;
        }
    }
    std::cout << "--- END " << label << " ---" << std::endl;
}

static void
process(char const* filename, bool include_ignorable, size_t max_len)
{
    std::shared_ptr<InputSource> is;

    // Tokenize file, skipping streams
    auto* fis = new FileInputSource(filename);
    is = std::shared_ptr<InputSource>(fis);
    dump_tokens(is, "FILE", max_len, include_ignorable, true, false);

    // Tokenize content streams, skipping inline images
    QPDF qpdf;
    qpdf.processFile(filename);
    int pageno = 0;
    for (auto& page: QPDFPageDocumentHelper(qpdf).getAllPages()) {
        ++pageno;
        Pl_Buffer plb("buffer");
        page.pipeContents(&plb);
        auto content_data = plb.getBufferSharedPointer();
        auto* bis = new BufferInputSource("content data", content_data.get());
        is = std::shared_ptr<InputSource>(bis);
        dump_tokens(
            is,
            "PAGE " + QUtil::int_to_string(pageno),
            max_len,
            include_ignorable,
            false,
            true);
    }

    // Tokenize object streams
    for (auto& obj: qpdf.getAllObjects()) {
        if (obj.isStream() && obj.getDict().getKey("/Type").isName() &&
            obj.getDict().getKey("/Type").getName() == "/ObjStm") {
            std::shared_ptr<Buffer> b = obj.getStreamData(qpdf_dl_specialized);
            auto* bis = new BufferInputSource("object stream data", b.get());
            is = std::shared_ptr<InputSource>(bis);
            dump_tokens(
                is,
                "OBJECT STREAM " + QUtil::int_to_string(obj.getObjectID()),
                max_len,
                include_ignorable,
                false,
                false);
        }
    }
}

int
main(int argc, char* argv[])
{
    QUtil::setLineBuf(stdout);
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    char const* filename = nullptr;
    size_t max_len = 0;
    bool include_ignorable = true;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-maxlen") == 0) {
                if (++i >= argc) {
                    usage();
                }
                max_len = QUtil::string_to_uint(argv[i]);
            } else if (strcmp(argv[i], "-no-ignorable") == 0) {
                include_ignorable = false;
            } else {
                usage();
            }
        } else if (filename) {
            usage();
        } else {
            filename = argv[i];
        }
    }
    if (filename == nullptr) {
        usage();
    }

    try {
        process(filename, include_ignorable, max_len);
    } catch (std::exception& e) {
        std::cerr << whoami << ": exception: " << e.what();
        exit(2);
    }
    return 0;
}

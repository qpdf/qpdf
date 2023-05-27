#include <qpdf/ContentNormalizer.hh>

#include <qpdf/QUtil.hh>

ContentNormalizer::ContentNormalizer() :
    any_bad_tokens(false),
    last_token_was_bad(false)
{
}

void
ContentNormalizer::handleToken(QPDFTokenizer::Token const& token)
{
    std::string value = token.getRawValue();
    QPDFTokenizer::token_type_e token_type = token.getType();

    if (token_type == QPDFTokenizer::tt_bad) {
        this->any_bad_tokens = true;
        this->last_token_was_bad = true;
    } else if (token_type != QPDFTokenizer::tt_eof) {
        this->last_token_was_bad = false;
    }

    switch (token_type) {
    case QPDFTokenizer::tt_space:
        {
            size_t len = value.length();
            for (size_t i = 0; i < len; ++i) {
                char ch = value.at(i);
                if (ch == '\r') {
                    if ((i + 1 < len) && (value.at(i + 1) == '\n')) {
                        // ignore
                    } else {
                        write("\n");
                    }
                } else {
                    write(&ch, 1);
                }
            }
        }
        break;

    case QPDFTokenizer::tt_string:
        // Replacing string and name tokens in this way normalizes their representation as this will
        // automatically handle quoting of unprintable characters, etc.
        writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_string, token.getValue()));
        break;

    case QPDFTokenizer::tt_name:
        writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_name, token.getValue()));
        break;

    default:
        writeToken(token);
        break;
    }

    value = token.getRawValue();
    if (((token_type == QPDFTokenizer::tt_string) || (token_type == QPDFTokenizer::tt_name)) &&
        ((value.find('\r') != std::string::npos) || (value.find('\n') != std::string::npos))) {
        write("\n");
    }
}

bool
ContentNormalizer::anyBadTokens() const
{
    return this->any_bad_tokens;
}

bool
ContentNormalizer::lastTokenWasBad() const
{
    return this->last_token_was_bad;
}

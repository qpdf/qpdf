#include <qpdf/ContentNormalizer.hh>

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QUtil.hh>

using namespace qpdf;

void
ContentNormalizer::handleToken(QPDFTokenizer::Token const& token)
{
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
            std::string const& value = token.getRawValue();
            auto size = value.size();
            size_t pos = 0;
            auto r_pos = value.find('\r');
            while (r_pos != std::string::npos) {
                if (pos != r_pos) {
                    write(&value[pos], r_pos - pos);
                }
                if (++r_pos >= size) {
                    write("\n");
                    return;
                }
                if (value[r_pos] != '\n') {
                    write("\n");
                }
                pos = r_pos;
                r_pos = value.find('\r', pos);
            }
            if (pos < size) {
                write(&value[pos], size - pos);
            }
        }
        return;

    case QPDFTokenizer::tt_string:
        // Replacing string and name tokens in this way normalizes their representation as this will
        // automatically handle quoting of unprintable characters, etc.
        write(QPDFObjectHandle::newString(token.getValue()).unparse());
        break;

    case QPDFTokenizer::tt_name:
        write(Name::normalize(token.getValue()));
        break;

    default:
        writeToken(token);
        return;
    }

    // tt_string or tt_name
    std::string const& value = token.getRawValue();
    if (value.find('\r') != std::string::npos || value.find('\n') != std::string::npos) {
        write("\n");
    }
}

#include <qpdf/QPDFTokenizer.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and
// it's not worth the risk of including it in case it may accidentally
// be used.

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <stdlib.h>
#include <string.h>

static bool
is_delimiter(char ch)
{
    return (strchr(" \t\n\v\f\r()<>[]{}/%", ch) != nullptr);
}

namespace
{
    class QPDFWordTokenFinder: public InputSource::Finder
    {
      public:
        QPDFWordTokenFinder(
            std::shared_ptr<InputSource> is, std::string const& str) :
            is(is),
            str(str)
        {
        }
        virtual ~QPDFWordTokenFinder() = default;
        virtual bool check();

      private:
        std::shared_ptr<InputSource> is;
        std::string str;
    };
} // namespace

bool
QPDFWordTokenFinder::check()
{
    // Find a word token matching the given string, preceded by a
    // delimiter, and followed by a delimiter or EOF.
    QPDFTokenizer tokenizer;
    QPDFTokenizer::Token t = tokenizer.readToken(is, "finder", true);
    qpdf_offset_t pos = is->tell();
    if (!(t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, str))) {
        QTC::TC("qpdf", "QPDFTokenizer finder found wrong word");
        return false;
    }
    qpdf_offset_t token_start = is->getLastOffset();
    char next;
    bool next_okay = false;
    if (is->read(&next, 1) == 0) {
        QTC::TC("qpdf", "QPDFTokenizer inline image at EOF");
        next_okay = true;
    } else {
        next_okay = is_delimiter(next);
    }
    is->seek(pos, SEEK_SET);
    if (!next_okay) {
        return false;
    }
    if (token_start == 0) {
        // Can't actually happen...we never start the search at the
        // beginning of the input.
        return false;
    }
    return true;
}

void
QPDFTokenizer::reset()
{
    state = st_top;
    type = tt_bad;
    val.clear();
    raw_val.clear();
    error_message = "";
    unread_char = false;
    char_to_unread = '\0';
    inline_image_bytes = 0;
    string_depth = 0;
    string_ignoring_newline = false;
    last_char_was_bs = false;
    last_char_was_cr = false;
}

QPDFTokenizer::Token::Token(token_type_e type, std::string const& value) :
    type(type),
    value(value),
    raw_value(value)
{
    if (type == tt_string) {
        raw_value = QPDFObjectHandle::newString(value).unparse();
    } else if (type == tt_name) {
        raw_value = QPDFObjectHandle::newName(value).unparse();
    }
}

QPDFTokenizer::QPDFTokenizer() :
    allow_eof(false),
    include_ignorable(false)
{
    reset();
}

void
QPDFTokenizer::allowEOF()
{
    this->allow_eof = true;
}

void
QPDFTokenizer::includeIgnorable()
{
    this->include_ignorable = true;
}

bool
QPDFTokenizer::isSpace(char ch)
{
    return ((ch == '\0') || QUtil::is_space(ch));
}

bool
QPDFTokenizer::isDelimiter(char ch)
{
    return is_delimiter(ch);
}

void
QPDFTokenizer::resolveLiteral()
{
    if ((this->val.length() > 0) && (this->val.at(0) == '/')) {
        this->type = tt_name;
        // Deal with # in name token.  Note: '/' by itself is a
        // valid name, so don't strip leading /.  That way we
        // don't have to deal with the empty string as a name.
        std::string nval = "/";
        size_t len = this->val.length();
        for (size_t i = 1; i < len; ++i) {
            char ch = this->val.at(i);
            if (ch == '#') {
                if ((i + 2 < len) && QUtil::is_hex_digit(this->val.at(i + 1)) &&
                    QUtil::is_hex_digit(this->val.at(i + 2))) {
                    char num[3];
                    num[0] = this->val.at(i + 1);
                    num[1] = this->val.at(i + 2);
                    num[2] = '\0';
                    char ch2 = static_cast<char>(strtol(num, nullptr, 16));
                    if (ch2 == '\0') {
                        this->type = tt_bad;
                        QTC::TC("qpdf", "QPDFTokenizer null in name");
                        this->error_message =
                            "null character not allowed in name token";
                        nval += "#00";
                    } else {
                        nval.append(1, ch2);
                    }
                    i += 2;
                } else {
                    QTC::TC("qpdf", "QPDFTokenizer bad name");
                    this->error_message =
                        "name with stray # will not work with PDF >= 1.2";
                    // Use null to encode a bad # -- this is reversed
                    // in QPDF_Name::normalizeName.
                    nval += '\0';
                }
            } else {
                nval.append(1, ch);
            }
        }
        this->val.clear();
        this->val += nval;
    } else if (QUtil::is_number(this->val.c_str())) {
        if (this->val.find('.') != std::string::npos) {
            this->type = tt_real;
        } else {
            this->type = tt_integer;
        }
    } else if ((this->val == "true") || (this->val == "false")) {
        this->type = tt_bool;
    } else if (this->val == "null") {
        this->type = tt_null;
    } else {
        // I don't really know what it is, so leave it as tt_word.
        // Lots of cases ($, #, etc.) other than actual words fall
        // into this category, but that's okay at least for now.
        this->type = tt_word;
    }
}

void
QPDFTokenizer::presentCharacter(char ch)
{
    char orig_ch = ch;

    handleCharacter(ch);

    if ((this->state == st_token_ready) && (this->type == tt_word)) {
        resolveLiteral();
    }

    if (!(betweenTokens() ||
          ((this->state == st_token_ready) && this->unread_char))) {
        this->raw_val += orig_ch;
    }
}

void
QPDFTokenizer::handleCharacter(char ch)
{
    // State machine is implemented such that some characters may be
    // handled more than once.  This happens whenever you have to use
    // the character that caused a state change in the new state.

    switch (this->state) {
    case (st_token_ready):
        throw std::logic_error(
            "INTERNAL ERROR: QPDF tokenizer presented character "
            "while token is waiting");

    case (st_top):
        // Note: we specifically do not use ctype here.  It is
        // locale-dependent.
        if (isSpace(ch)) {
            if (this->include_ignorable) {
                this->state = st_in_space;
                this->val += ch;
            }
            return;
        }
        switch (ch) {
        case '%':
            this->state = st_in_comment;
            if (this->include_ignorable) {
                this->val += ch;
            }
            return;

        case '(':
            this->string_depth = 1;
            this->string_ignoring_newline = false;
            memset(this->bs_num_register, '\0', sizeof(this->bs_num_register));
            this->last_char_was_bs = false;
            this->last_char_was_cr = false;
            this->state = st_in_string;
            return;

        case '<':
            this->state = st_lt;
            return;

        case '>':
            this->state = st_gt;
            return;

        default:
            this->val += ch;
            switch (ch) {
            case ')':
                this->type = tt_bad;
                QTC::TC("qpdf", "QPDFTokenizer bad )");
                this->error_message = "unexpected )";
                this->state = st_token_ready;
                return;

            case '[':
                this->type = tt_array_open;
                this->state = st_token_ready;
                return;

            case ']':
                this->type = tt_array_close;
                this->state = st_token_ready;
                return;

            case '{':
                this->type = tt_brace_open;
                this->state = st_token_ready;
                return;

            case '}':
                this->type = tt_brace_close;
                this->state = st_token_ready;
                return;

            default:
                this->state = st_literal;
                return;
            }
        }

    case st_in_space:
        // We only enter this state if include_ignorable is true.
        if (!isSpace(ch)) {
            this->type = tt_space;
            this->unread_char = true;
            this->char_to_unread = ch;
            this->state = st_token_ready;
        } else {
            this->val += ch;
        }
        return;

    case st_in_comment:
        if ((ch == '\r') || (ch == '\n')) {
            if (this->include_ignorable) {
                this->type = tt_comment;
                this->unread_char = true;
                this->char_to_unread = ch;
                this->state = st_token_ready;
            } else {
                this->state = st_top;
            }
        } else if (this->include_ignorable) {
            this->val += ch;
        }
        return;

    case st_lt:
        if (ch == '<') {
            this->val += "<<";
            this->type = tt_dict_open;
            this->state = st_token_ready;
            return;
        }
        this->state = st_in_hexstring;
        inHexstring(ch);
        return;

    case st_gt:
        if (ch == '>') {
            this->val += ">>";
            this->type = tt_dict_close;
            this->state = st_token_ready;
        } else {
            this->val += ">";
            this->type = tt_bad;
            QTC::TC("qpdf", "QPDFTokenizer bad >");
            this->error_message = "unexpected >";
            this->unread_char = true;
            this->char_to_unread = ch;
            this->state = st_token_ready;
        }
        return;

    case st_in_string:
        {
            if (this->string_ignoring_newline && (ch != '\n')) {
                this->string_ignoring_newline = false;
            }

            size_t bs_num_count = strlen(this->bs_num_register);
            bool ch_is_octal = ((ch >= '0') && (ch <= '7'));
            if ((bs_num_count == 3) || ((bs_num_count > 0) && (!ch_is_octal))) {
                // We've accumulated \ddd.  PDF Spec says to ignore
                // high-order overflow.
                this->val += static_cast<char>(
                    strtol(this->bs_num_register, nullptr, 8));
                memset(
                    this->bs_num_register, '\0', sizeof(this->bs_num_register));
                bs_num_count = 0;
            }

            inString(ch, bs_num_count);

            this->last_char_was_cr =
                ((!this->string_ignoring_newline) && (ch == '\r'));
            this->last_char_was_bs =
                ((!this->last_char_was_bs) && (ch == '\\'));
        }
        return;

    case st_literal:
        if (isDelimiter(ch)) {
            // A C-locale whitespace character or delimiter terminates
            // token.  It is important to unread the whitespace
            // character even though it is ignored since it may be the
            // newline after a stream keyword.  Removing it here could
            // make the stream-reading code break on some files,
            // though not on any files in the test suite as of this
            // writing.

            this->type = tt_word;
            this->unread_char = true;
            this->char_to_unread = ch;
            this->state = st_token_ready;
        } else {
            this->val += ch;
        }
        return;

    case st_inline_image:
        this->val += ch;
        if (this->val.length() == this->inline_image_bytes) {
            QTC::TC("qpdf", "QPDFTokenizer found EI by byte count");
            this->type = tt_inline_image;
            this->inline_image_bytes = 0;
            this->state = st_token_ready;
        }
        return;

    case (st_in_hexstring):
        inHexstring(ch);
        return;

    default:
        throw std::logic_error(
            "INTERNAL ERROR: invalid state while reading token");
    }
}

void
QPDFTokenizer::inHexstring(char ch)
{
    if (ch == '>') {
        this->type = tt_string;
        this->state = st_token_ready;
        if (this->val.length() % 2) {
            // PDF spec says odd hexstrings have implicit
            // trailing 0.
            this->val += '0';
        }
        char num[3];
        num[2] = '\0';
        std::string nval;
        for (unsigned int i = 0; i < this->val.length(); i += 2) {
            num[0] = this->val.at(i);
            num[1] = this->val.at(i + 1);
            char nch = static_cast<char>(strtol(num, nullptr, 16));
            nval += nch;
        }
        this->val.clear();
        this->val += nval;
    } else if (QUtil::is_hex_digit(ch)) {
        this->val += ch;
    } else if (isSpace(ch)) {
        // ignore
    } else {
        this->type = tt_bad;
        QTC::TC("qpdf", "QPDFTokenizer bad hexstring character");
        this->error_message =
            std::string("invalid character (") + ch + ") in hexstring";
        this->state = st_token_ready;
    }
}

void
QPDFTokenizer::inString(char ch, size_t bs_num_count)
{
    bool ch_is_octal = ((ch >= '0') && (ch <= '7'));
    if (this->string_ignoring_newline && (ch == '\n')) {
        // ignore
        this->string_ignoring_newline = false;
        return;
    } else if (ch_is_octal && (this->last_char_was_bs || (bs_num_count > 0))) {
        this->bs_num_register[bs_num_count++] = ch;
        return;
    } else if (this->last_char_was_bs) {
        switch (ch) {
        case 'n':
            this->val += '\n';
            return;

        case 'r':
            this->val += '\r';
            return;

        case 't':
            this->val += '\t';
            return;

        case 'b':
            this->val += '\b';
            return;

        case 'f':
            this->val += '\f';
            return;

        case '\n':
            return;

        case '\r':
            this->string_ignoring_newline = true;
            return;

        default:
            // PDF spec says backslash is ignored before anything else
            this->val += ch;
            return;
        }
    } else if (ch == '\\') {
        // last_char_was_bs is set/cleared below as appropriate
        if (bs_num_count) {
            throw std::logic_error(
                "INTERNAL ERROR: QPDFTokenizer: bs_num_count != 0 "
                "when ch == '\\'");
        }
    } else if (ch == '(') {
        this->val += ch;
        ++this->string_depth;
        return;
    } else if ((ch == ')') && (--this->string_depth == 0)) {
        this->type = tt_string;
        this->state = st_token_ready;
        return;
    } else if (ch == '\r') {
        // CR by itself is converted to LF
        this->val += '\n';
        return;
    } else if (ch == '\n') {
        // CR LF is converted to LF
        if (!this->last_char_was_cr) {
            this->val += ch;
        }
        return;
    } else {
        this->val += ch;
        return;
    }
}

void
QPDFTokenizer::presentEOF()
{
    if (this->state == st_literal) {
        QTC::TC("qpdf", "QPDFTokenizer EOF reading appendable token");
        resolveLiteral();
    } else if ((this->include_ignorable) && (this->state == st_in_space)) {
        this->type = tt_space;
    } else if ((this->include_ignorable) && (this->state == st_in_comment)) {
        this->type = tt_comment;
    } else if (betweenTokens()) {
        this->type = tt_eof;
    } else if (this->state != st_token_ready) {
        QTC::TC("qpdf", "QPDFTokenizer EOF reading token");
        this->type = tt_bad;
        this->error_message = "EOF while reading token";
    }

    this->state = st_token_ready;
}

void
QPDFTokenizer::expectInlineImage(std::shared_ptr<InputSource> input)
{
    if (this->state != st_top) {
        throw std::logic_error("QPDFTokenizer::expectInlineImage called"
                               " when tokenizer is in improper state");
    }
    findEI(input);
    this->state = st_inline_image;
}

void
QPDFTokenizer::findEI(std::shared_ptr<InputSource> input)
{
    if (!input.get()) {
        return;
    }

    qpdf_offset_t last_offset = input->getLastOffset();
    qpdf_offset_t pos = input->tell();

    // Use QPDFWordTokenFinder to find EI surrounded by delimiters.
    // Then read the next several tokens or up to EOF. If we find any
    // suspicious-looking or tokens, this is probably still part of
    // the image data, so keep looking for EI. Stop at the first EI
    // that passes. If we get to the end without finding one, return
    // the last EI we found. Store the number of bytes expected in the
    // inline image including the EI and use that to break out of
    // inline image, falling back to the old method if needed.

    bool okay = false;
    bool first_try = true;
    while (!okay) {
        QPDFWordTokenFinder f(input, "EI");
        if (!input->findFirst("EI", input->tell(), 0, f)) {
            break;
        }
        this->inline_image_bytes = QIntC::to_size(input->tell() - pos - 2);

        QPDFTokenizer check;
        bool found_bad = false;
        // Look at the next 10 tokens or up to EOF. The next inline
        // image's image data would look like bad tokens, but there
        // will always be at least 10 tokens between one inline
        // image's EI and the next valid one's ID since width, height,
        // bits per pixel, and color space are all required as well as
        // a BI and ID. If we get 10 good tokens in a row or hit EOF,
        // we can be pretty sure we've found the actual EI.
        for (int i = 0; i < 10; ++i) {
            QPDFTokenizer::Token t = check.readToken(input, "checker", true);
            token_type_e type = t.getType();
            if (type == tt_eof) {
                okay = true;
            } else if (type == tt_bad) {
                found_bad = true;
            } else if (type == tt_word) {
                // The qpdf tokenizer lumps alphabetic and otherwise
                // uncategorized characters into "words". We recognize
                // strings of alphabetic characters as potential valid
                // operators for purposes of telling whether we're in
                // valid content or not. It's not perfect, but it
                // should work more reliably than what we used to do,
                // which was already good enough for the vast majority
                // of files.
                bool found_alpha = false;
                bool found_non_printable = false;
                bool found_other = false;
                for (char ch: t.getValue()) {
                    if (((ch >= 'a') && (ch <= 'z')) ||
                        ((ch >= 'A') && (ch <= 'Z')) || (ch == '*')) {
                        // Treat '*' as alpha since there are valid
                        // PDF operators that contain * along with
                        // alphabetic characters.
                        found_alpha = true;
                    } else if (
                        (static_cast<signed char>(ch) < 32) && (!isSpace(ch))) {
                        // Compare ch as a signed char so characters
                        // outside of 7-bit will be < 0.
                        found_non_printable = true;
                        break;
                    } else {
                        found_other = true;
                    }
                }
                if (found_non_printable || (found_alpha && found_other)) {
                    found_bad = true;
                }
            }
            if (okay || found_bad) {
                break;
            }
        }
        if (!found_bad) {
            okay = true;
        }
        if (!okay) {
            first_try = false;
        }
    }
    if (okay && (!first_try)) {
        QTC::TC("qpdf", "QPDFTokenizer found EI after more than one try");
    }

    input->seek(pos, SEEK_SET);
    input->setLastOffset(last_offset);
}

bool
QPDFTokenizer::getToken(Token& token, bool& unread_char, char& ch)
{
    bool ready = (this->state == st_token_ready);
    unread_char = this->unread_char;
    ch = this->char_to_unread;
    if (ready) {
        if (this->type == tt_bad) {
            this->val.clear();
            this->val += this->raw_val;
        }
        token =
            Token(this->type, this->val, this->raw_val, this->error_message);
        this->reset();
    }
    return ready;
}

bool
QPDFTokenizer::betweenTokens()
{
    return (
        (this->state == st_top) ||
        ((!this->include_ignorable) &&
         ((this->state == st_in_comment) || (this->state == st_in_space))));
}

QPDFTokenizer::Token
QPDFTokenizer::readToken(
    std::shared_ptr<InputSource> input,
    std::string const& context,
    bool allow_bad,
    size_t max_len)
{
    qpdf_offset_t offset = input->tell();
    Token token;
    bool unread_char;
    char char_to_unread;
    bool presented_eof = false;
    while (!getToken(token, unread_char, char_to_unread)) {
        char ch;
        if (input->read(&ch, 1) == 0) {
            if (!presented_eof) {
                presentEOF();
                presented_eof = true;
                if ((this->type == tt_eof) && (!this->allow_eof)) {
                    // Nothing in the qpdf library calls readToken
                    // without allowEOF anymore, so this case is not
                    // exercised.
                    this->type = tt_bad;
                    this->error_message = "unexpected EOF";
                    offset = input->getLastOffset();
                }
            } else {
                throw std::logic_error(
                    "getToken returned false after presenting EOF");
            }
        } else {
            presentCharacter(ch);
            if (betweenTokens() && (input->getLastOffset() == offset)) {
                ++offset;
            }
            if (max_len && (this->raw_val.length() >= max_len) &&
                (this->state != st_token_ready)) {
                // terminate this token now
                QTC::TC("qpdf", "QPDFTokenizer block long token");
                this->type = tt_bad;
                this->state = st_token_ready;
                this->error_message =
                    "exceeded allowable length while reading token";
            }
        }
    }

    if (unread_char) {
        input->unreadCh(char_to_unread);
    }

    if (token.getType() != tt_eof) {
        input->setLastOffset(offset);
    }

    if (token.getType() == tt_bad) {
        if (allow_bad) {
            QTC::TC("qpdf", "QPDFTokenizer allowing bad token");
        } else {
            throw QPDFExc(
                qpdf_e_damaged_pdf,
                input->getName(),
                context,
                offset,
                token.getErrorMessage());
        }
    }

    return token;
}

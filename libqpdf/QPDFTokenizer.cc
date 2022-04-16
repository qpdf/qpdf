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
    return (strchr(" \t\n\v\f\r()<>[]{}/%", ch) != 0);
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

QPDFTokenizer::Members::Members() :
    allow_eof(false),
    include_ignorable(false)
{
    reset();
}

void
QPDFTokenizer::Members::reset()
{
    state = st_top;
    type = tt_bad;
    val = "";
    raw_val = "";
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
    m(new Members())
{
}

void
QPDFTokenizer::allowEOF()
{
    this->m->allow_eof = true;
}

void
QPDFTokenizer::includeIgnorable()
{
    this->m->include_ignorable = true;
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
    if ((this->m->val.length() > 0) && (this->m->val.at(0) == '/')) {
        this->m->type = tt_name;
        // Deal with # in name token.  Note: '/' by itself is a
        // valid name, so don't strip leading /.  That way we
        // don't have to deal with the empty string as a name.
        std::string nval = "/";
        size_t len = this->m->val.length();
        for (size_t i = 1; i < len; ++i) {
            char ch = this->m->val.at(i);
            if (ch == '#') {
                if ((i + 2 < len) &&
                    QUtil::is_hex_digit(this->m->val.at(i + 1)) &&
                    QUtil::is_hex_digit(this->m->val.at(i + 2))) {
                    char num[3];
                    num[0] = this->m->val.at(i + 1);
                    num[1] = this->m->val.at(i + 2);
                    num[2] = '\0';
                    char ch2 = static_cast<char>(strtol(num, 0, 16));
                    if (ch2 == '\0') {
                        this->m->type = tt_bad;
                        QTC::TC("qpdf", "QPDFTokenizer null in name");
                        this->m->error_message =
                            "null character not allowed in name token";
                        nval += "#00";
                    } else {
                        nval.append(1, ch2);
                    }
                    i += 2;
                } else {
                    QTC::TC("qpdf", "QPDFTokenizer bad name");
                    this->m->error_message =
                        "name with stray # will not work with PDF >= 1.2";
                    // Use null to encode a bad # -- this is reversed
                    // in QPDF_Name::normalizeName.
                    nval += '\0';
                }
            } else {
                nval.append(1, ch);
            }
        }
        this->m->val = nval;
    } else if (QUtil::is_number(this->m->val.c_str())) {
        if (this->m->val.find('.') != std::string::npos) {
            this->m->type = tt_real;
        } else {
            this->m->type = tt_integer;
        }
    } else if ((this->m->val == "true") || (this->m->val == "false")) {
        this->m->type = tt_bool;
    } else if (this->m->val == "null") {
        this->m->type = tt_null;
    } else {
        // I don't really know what it is, so leave it as tt_word.
        // Lots of cases ($, #, etc.) other than actual words fall
        // into this category, but that's okay at least for now.
        this->m->type = tt_word;
    }
}

void
QPDFTokenizer::presentCharacter(char ch)
{
    if (this->m->state == st_token_ready) {
        throw std::logic_error(
            "INTERNAL ERROR: QPDF tokenizer presented character "
            "while token is waiting");
    }

    char orig_ch = ch;

    // State machine is implemented such that some characters may be
    // handled more than once.  This happens whenever you have to use
    // the character that caused a state change in the new state.

    bool handled = true;
    if (this->m->state == st_top) {
        // Note: we specifically do not use ctype here.  It is
        // locale-dependent.
        if (isSpace(ch)) {
            if (this->m->include_ignorable) {
                this->m->state = st_in_space;
                this->m->val += ch;
            }
        } else if (ch == '%') {
            this->m->state = st_in_comment;
            if (this->m->include_ignorable) {
                this->m->val += ch;
            }
        } else if (ch == '(') {
            this->m->string_depth = 1;
            this->m->string_ignoring_newline = false;
            memset(
                this->m->bs_num_register,
                '\0',
                sizeof(this->m->bs_num_register));
            this->m->last_char_was_bs = false;
            this->m->last_char_was_cr = false;
            this->m->state = st_in_string;
        } else if (ch == '<') {
            this->m->state = st_lt;
        } else if (ch == '>') {
            this->m->state = st_gt;
        } else {
            this->m->val += ch;
            if (ch == ')') {
                this->m->type = tt_bad;
                QTC::TC("qpdf", "QPDFTokenizer bad )");
                this->m->error_message = "unexpected )";
                this->m->state = st_token_ready;
            } else if (ch == '[') {
                this->m->type = tt_array_open;
                this->m->state = st_token_ready;
            } else if (ch == ']') {
                this->m->type = tt_array_close;
                this->m->state = st_token_ready;
            } else if (ch == '{') {
                this->m->type = tt_brace_open;
                this->m->state = st_token_ready;
            } else if (ch == '}') {
                this->m->type = tt_brace_close;
                this->m->state = st_token_ready;
            } else {
                this->m->state = st_literal;
            }
        }
    } else if (this->m->state == st_in_space) {
        // We only enter this state if include_ignorable is true.
        if (!isSpace(ch)) {
            this->m->type = tt_space;
            this->m->unread_char = true;
            this->m->char_to_unread = ch;
            this->m->state = st_token_ready;
        } else {
            this->m->val += ch;
        }
    } else if (this->m->state == st_in_comment) {
        if ((ch == '\r') || (ch == '\n')) {
            if (this->m->include_ignorable) {
                this->m->type = tt_comment;
                this->m->unread_char = true;
                this->m->char_to_unread = ch;
                this->m->state = st_token_ready;
            } else {
                this->m->state = st_top;
            }
        } else if (this->m->include_ignorable) {
            this->m->val += ch;
        }
    } else if (this->m->state == st_lt) {
        if (ch == '<') {
            this->m->val = "<<";
            this->m->type = tt_dict_open;
            this->m->state = st_token_ready;
        } else {
            handled = false;
            this->m->state = st_in_hexstring;
        }
    } else if (this->m->state == st_gt) {
        if (ch == '>') {
            this->m->val = ">>";
            this->m->type = tt_dict_close;
            this->m->state = st_token_ready;
        } else {
            this->m->val = ">";
            this->m->type = tt_bad;
            QTC::TC("qpdf", "QPDFTokenizer bad >");
            this->m->error_message = "unexpected >";
            this->m->unread_char = true;
            this->m->char_to_unread = ch;
            this->m->state = st_token_ready;
        }
    } else if (this->m->state == st_in_string) {
        if (this->m->string_ignoring_newline && (ch != '\n')) {
            this->m->string_ignoring_newline = false;
        }

        size_t bs_num_count = strlen(this->m->bs_num_register);
        bool ch_is_octal = ((ch >= '0') && (ch <= '7'));
        if ((bs_num_count == 3) || ((bs_num_count > 0) && (!ch_is_octal))) {
            // We've accumulated \ddd.  PDF Spec says to ignore
            // high-order overflow.
            this->m->val +=
                static_cast<char>(strtol(this->m->bs_num_register, 0, 8));
            memset(
                this->m->bs_num_register,
                '\0',
                sizeof(this->m->bs_num_register));
            bs_num_count = 0;
        }

        if (this->m->string_ignoring_newline && (ch == '\n')) {
            // ignore
            this->m->string_ignoring_newline = false;
        } else if (
            ch_is_octal && (this->m->last_char_was_bs || (bs_num_count > 0))) {
            this->m->bs_num_register[bs_num_count++] = ch;
        } else if (this->m->last_char_was_bs) {
            switch (ch) {
            case 'n':
                this->m->val += '\n';
                break;

            case 'r':
                this->m->val += '\r';
                break;

            case 't':
                this->m->val += '\t';
                break;

            case 'b':
                this->m->val += '\b';
                break;

            case 'f':
                this->m->val += '\f';
                break;

            case '\n':
                break;

            case '\r':
                this->m->string_ignoring_newline = true;
                break;

            default:
                // PDF spec says backslash is ignored before anything else
                this->m->val += ch;
                break;
            }
        } else if (ch == '\\') {
            // last_char_was_bs is set/cleared below as appropriate
            if (bs_num_count) {
                throw std::logic_error(
                    "INTERNAL ERROR: QPDFTokenizer: bs_num_count != 0 "
                    "when ch == '\\'");
            }
        } else if (ch == '(') {
            this->m->val += ch;
            ++this->m->string_depth;
        } else if ((ch == ')') && (--this->m->string_depth == 0)) {
            this->m->type = tt_string;
            this->m->state = st_token_ready;
        } else if (ch == '\r') {
            // CR by itself is converted to LF
            this->m->val += '\n';
        } else if (ch == '\n') {
            // CR LF is converted to LF
            if (!this->m->last_char_was_cr) {
                this->m->val += ch;
            }
        } else {
            this->m->val += ch;
        }

        this->m->last_char_was_cr =
            ((!this->m->string_ignoring_newline) && (ch == '\r'));
        this->m->last_char_was_bs =
            ((!this->m->last_char_was_bs) && (ch == '\\'));
    } else if (this->m->state == st_literal) {
        if (isDelimiter(ch)) {
            // A C-locale whitespace character or delimiter terminates
            // token.  It is important to unread the whitespace
            // character even though it is ignored since it may be the
            // newline after a stream keyword.  Removing it here could
            // make the stream-reading code break on some files,
            // though not on any files in the test suite as of this
            // writing.

            this->m->type = tt_word;
            this->m->unread_char = true;
            this->m->char_to_unread = ch;
            this->m->state = st_token_ready;
        } else {
            this->m->val += ch;
        }
    } else if (this->m->state == st_inline_image) {
        this->m->val += ch;
        size_t len = this->m->val.length();
        if (len == this->m->inline_image_bytes) {
            QTC::TC("qpdf", "QPDFTokenizer found EI by byte count");
            this->m->type = tt_inline_image;
            this->m->inline_image_bytes = 0;
            this->m->state = st_token_ready;
        }
    } else {
        handled = false;
    }

    if (handled) {
        // okay
    } else if (this->m->state == st_in_hexstring) {
        if (ch == '>') {
            this->m->type = tt_string;
            this->m->state = st_token_ready;
            if (this->m->val.length() % 2) {
                // PDF spec says odd hexstrings have implicit
                // trailing 0.
                this->m->val += '0';
            }
            char num[3];
            num[2] = '\0';
            std::string nval;
            for (unsigned int i = 0; i < this->m->val.length(); i += 2) {
                num[0] = this->m->val.at(i);
                num[1] = this->m->val.at(i + 1);
                char nch = static_cast<char>(strtol(num, 0, 16));
                nval += nch;
            }
            this->m->val = nval;
        } else if (QUtil::is_hex_digit(ch)) {
            this->m->val += ch;
        } else if (isSpace(ch)) {
            // ignore
        } else {
            this->m->type = tt_bad;
            QTC::TC("qpdf", "QPDFTokenizer bad hexstring character");
            this->m->error_message =
                std::string("invalid character (") + ch + ") in hexstring";
            this->m->state = st_token_ready;
        }
    } else {
        throw std::logic_error(
            "INTERNAL ERROR: invalid state while reading token");
    }

    if ((this->m->state == st_token_ready) && (this->m->type == tt_word)) {
        resolveLiteral();
    }

    if (!(betweenTokens() ||
          ((this->m->state == st_token_ready) && this->m->unread_char))) {
        this->m->raw_val += orig_ch;
    }
}

void
QPDFTokenizer::presentEOF()
{
    if (this->m->state == st_literal) {
        QTC::TC("qpdf", "QPDFTokenizer EOF reading appendable token");
        resolveLiteral();
    } else if (
        (this->m->include_ignorable) && (this->m->state == st_in_space)) {
        this->m->type = tt_space;
    } else if (
        (this->m->include_ignorable) && (this->m->state == st_in_comment)) {
        this->m->type = tt_comment;
    } else if (betweenTokens()) {
        this->m->type = tt_eof;
    } else if (this->m->state != st_token_ready) {
        QTC::TC("qpdf", "QPDFTokenizer EOF reading token");
        this->m->type = tt_bad;
        this->m->error_message = "EOF while reading token";
    }

    this->m->state = st_token_ready;
}

void
QPDFTokenizer::expectInlineImage(std::shared_ptr<InputSource> input)
{
    if (this->m->state != st_top) {
        throw std::logic_error("QPDFTokenizer::expectInlineImage called"
                               " when tokenizer is in improper state");
    }
    findEI(input);
    this->m->state = st_inline_image;
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
        this->m->inline_image_bytes = QIntC::to_size(input->tell() - pos - 2);

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
                std::string value = t.getValue();
                for (std::string::iterator iter = value.begin();
                     iter != value.end();
                     ++iter) {
                    char ch = *iter;
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
    bool ready = (this->m->state == st_token_ready);
    unread_char = this->m->unread_char;
    ch = this->m->char_to_unread;
    if (ready) {
        if (this->m->type == tt_bad) {
            this->m->val = this->m->raw_val;
        }
        token = Token(
            this->m->type,
            this->m->val,
            this->m->raw_val,
            this->m->error_message);
        this->m->reset();
    }
    return ready;
}

bool
QPDFTokenizer::betweenTokens()
{
    return (
        (this->m->state == st_top) ||
        ((!this->m->include_ignorable) &&
         ((this->m->state == st_in_comment) ||
          (this->m->state == st_in_space))));
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
                if ((this->m->type == tt_eof) && (!this->m->allow_eof)) {
                    // Nothing in the qpdf library calls readToken
                    // without allowEOF anymore, so this case is not
                    // exercised.
                    this->m->type = tt_bad;
                    this->m->error_message = "unexpected EOF";
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
            if (max_len && (this->m->raw_val.length() >= max_len) &&
                (this->m->state != st_token_ready)) {
                // terminate this token now
                QTC::TC("qpdf", "QPDFTokenizer block long token");
                this->m->type = tt_bad;
                this->m->state = st_token_ready;
                this->m->error_message =
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

#ifndef QPDFTOKENIZER_PRIVATE_HH
#define QPDFTOKENIZER_PRIVATE_HH

#include <qpdf/QPDFTokenizer.hh>

namespace qpdf
{

    class Tokenizer
    {
      public:
        Tokenizer();
        Tokenizer(Tokenizer const&) = delete;
        Tokenizer& operator=(Tokenizer const&) = delete;

        // Methods to support QPDFTokenizer. See QPDFTokenizer.hh for detail. Some of these are used
        // by Tokenizer internally but are not accessed directly by the rest of qpdf.

        void allowEOF();
        void includeIgnorable();
        void presentCharacter(char ch);
        void presentEOF();
        bool betweenTokens();

        // If a token is available, return true and initialize token with the token, unread_char
        // with whether or not we have to unread the last character, and if unread_char, ch with the
        // character to unread.
        bool getToken(QPDFTokenizer::Token& token, bool& unread_char, char& ch);

        // Read a token from an input source. Context describes the context in which the token is
        // being read and is used in the exception thrown if there is an error. After a token is
        // read, the position of the input source returned by input->tell() points to just after the
        // token, and the input source's "last offset" as returned by input->getLastOffset() points
        // to the beginning of the token.
        QPDFTokenizer::Token readToken(
            InputSource& input,
            std::string const& context,
            bool allow_bad = false,
            size_t max_len = 0);

        // Calling this method puts the tokenizer in a state for reading inline images. You should
        // call this method after reading the character following the ID operator. In that state, it
        // will return all data up to BUT NOT INCLUDING the next EI token. After you call this
        // method, the next call to readToken (or the token created next time getToken returns true)
        // will either be tt_inline_image or tt_bad. This is the only way readToken returns a
        // tt_inline_image token.
        void expectInlineImage(InputSource& input);

        // Read a token from an input source. Context describes the context in which the token is
        // being read and is used in the exception thrown if there is an error. After a token is
        // read, the position of the input source returned by input->tell() points to just after the
        // token, and the input source's "last offset" as returned by input->getLastOffset() points
        // to the beginning of the token. Returns false if the token is bad or if scanning produced
        // an error message for any reason.
        bool nextToken(InputSource& input, std::string const& context, size_t max_len = 0);

        // The following methods are only valid after nextToken has been called and until another
        // QPDFTokenizer method is called. They allow the results of calling nextToken to be
        // accessed without creating a Token, thus avoiding copying information that may not be
        // needed.

        inline QPDFTokenizer::token_type_e
        getType() const
        {
            return this->type;
        }
        inline std::string const&
        getValue() const
        {
            return (this->type == QPDFTokenizer::tt_name || this->type == QPDFTokenizer::tt_string)
                ? this->val
                : this->raw_val;
        }
        inline std::string const&
        getRawValue() const
        {
            return this->raw_val;
        }
        inline std::string const&
        getErrorMessage() const
        {
            return this->error_message;
        }

      private:
        bool isSpace(char);
        bool isDelimiter(char);
        void findEI(InputSource& input);

        enum state_e {
            st_top,
            st_in_hexstring,
            st_in_string,
            st_in_hexstring_2nd,
            st_name,
            st_literal,
            st_in_space,
            st_in_comment,
            st_string_escape,
            st_char_code,
            st_string_after_cr,
            st_lt,
            st_gt,
            st_inline_image,
            st_sign,
            st_number,
            st_real,
            st_decimal,
            st_name_hex1,
            st_name_hex2,
            st_before_token,
            st_token_ready
        };

        void handleCharacter(char);
        void inBeforeToken(char);
        void inTop(char);
        void inSpace(char);
        void inComment(char);
        void inString(char);
        void inName(char);
        void inLt(char);
        void inGt(char);
        void inStringAfterCR(char);
        void inStringEscape(char);
        void inLiteral(char);
        void inCharCode(char);
        void inHexstring(char);
        void inHexstring2nd(char);
        void inInlineImage(char);
        void inTokenReady(char);
        void inNameHex1(char);
        void inNameHex2(char);
        void inSign(char);
        void inDecimal(char);
        void inNumber(char);
        void inReal(char);
        void reset();

        // Lexer state
        state_e state;

        bool allow_eof{false};
        bool include_ignorable{false};

        // Current token accumulation
        QPDFTokenizer::token_type_e type;
        std::string val;
        std::string raw_val;
        std::string error_message;
        bool before_token;
        bool in_token;
        char char_to_unread;
        size_t inline_image_bytes;
        bool bad;

        // State for strings
        int string_depth;
        int char_code;
        char hex_char;
        int digit_count;
    };

} // namespace qpdf

#endif // QPDFTOKENIZER_PRIVATE_HH

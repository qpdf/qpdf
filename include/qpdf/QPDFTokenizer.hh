// Copyright (c) 2005-2018 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef QPDFTOKENIZER_HH
#define QPDFTOKENIZER_HH

#include <qpdf/DLL.h>

#include <qpdf/InputSource.hh>
#include <qpdf/PointerHolder.hh>
#include <string>
#include <stdio.h>

class QPDFTokenizer
{
  public:
    // Token type tt_eof is only returned of allowEOF() is called on
    // the tokenizer. tt_eof was introduced in QPDF version 4.1.
    // tt_space, tt_comment, and tt_inline_image were added in QPDF
    // version 8.
    enum token_type_e
    {
	tt_bad,
	tt_array_close,
	tt_array_open,
	tt_brace_close,
	tt_brace_open,
	tt_dict_close,
	tt_dict_open,
	tt_integer,
	tt_name,
	tt_real,
	tt_string,
	tt_null,
	tt_bool,
	tt_word,
        tt_eof,
        tt_space,
        tt_comment,
        tt_inline_image,
    };

    class Token
    {
      public:
	Token() : type(tt_bad) {}
        QPDF_DLL
	Token(token_type_e type, std::string const& value);
	Token(token_type_e type, std::string const& value,
	      std::string raw_value, std::string error_message) :
	    type(type),
	    value(value),
	    raw_value(raw_value),
	    error_message(error_message)
	{
	}
	token_type_e getType() const
	{
	    return this->type;
	}
	std::string const& getValue() const
	{
	    return this->value;
	}
	std::string const& getRawValue() const
	{
	    return this->raw_value;
	}
	std::string const& getErrorMessage() const
	{
	    return this->error_message;
	}
	bool operator==(Token const& rhs) const
	{
	    // Ignore fields other than type and value
	    return ((this->type != tt_bad) &&
		    (this->type == rhs.type) &&
		    (this->value == rhs.value));
	}

      private:
	token_type_e type;
	std::string value;
	std::string raw_value;
	std::string error_message;
    };

    QPDF_DLL
    QPDFTokenizer();

    // PDF files with version < 1.2 allowed the pound character
    // anywhere in a name.  Starting with version 1.2, the pound
    // character was allowed only when followed by two hexadecimal
    // digits.  This method should be called when parsing a PDF file
    // whose version is older than 1.2.
    QPDF_DLL
    void allowPoundAnywhereInName();

    // If called, treat EOF as a separate token type instead of an
    // error.  This was introduced in QPDF 4.1 to facilitate
    // tokenizing content streams.
    QPDF_DLL
    void allowEOF();

    // If called, readToken will return "ignorable" tokens for space
    // and comments. This was added in QPDF 8.
    QPDF_DLL
    void includeIgnorable();

    // There are two modes of operation: push and pull. The pull
    // method is easier but requires an input source. The push method
    // is more complicated but can be used to tokenize a stream of
    // incoming characters in a pipeline.

    // Push mode:

    // Keep presenting characters with presentCharacter() and
    // presentEOF() and calling getToken() until getToken() returns
    // true. When it does, be sure to check unread_ch and to unread ch
    // if it is true.

    // It these are called when a token is available, an exception
    // will be thrown.
    QPDF_DLL
    void presentCharacter(char ch);
    QPDF_DLL
    void presentEOF();

    // If a token is available, return true and initialize token with
    // the token, unread_char with whether or not we have to unread
    // the last character, and if unread_char, ch with the character
    // to unread.
    QPDF_DLL
    bool getToken(Token& token, bool& unread_char, char& ch);

    // This function returns true of the current character is between
    // tokens (i.e., white space that is not part of a string) or is
    // part of a comment.  A tokenizing filter can call this to
    // determine whether to output the character.
    QPDF_DLL
    bool betweenTokens();

    // Pull mode:

    // Read a token from an input source. Context describes the
    // context in which the token is being read and is used in the
    // exception thrown if there is an error. After a token is read,
    // the position of the input source returned by input->tell()
    // points to just after the token, and the input source's "last
    // offset" as returned by input->getLastOffset() points to the
    // beginning of the token.
    QPDF_DLL
    Token readToken(PointerHolder<InputSource> input,
                    std::string const& context,
                    bool allow_bad = false,
                    size_t max_len = 0);

    // Calling this method puts the tokenizer in a state for reading
    // inline images. In that state, it will return all data up to and
    // including the next EI token. After you call this method, the
    // next call to readToken (or the token created next time getToken
    // returns true) will either be tt_inline_image or tt_bad. This is
    // the only way readToken returns a tt_inline_image token.
    QPDF_DLL
    void expectInlineImage();

  private:
    // Do not implement copy or assignment
    QPDFTokenizer(QPDFTokenizer const&);
    QPDFTokenizer& operator=(QPDFTokenizer const&);

    void resolveLiteral();
    bool isSpace(char);
    bool isDelimiter(char);

    enum state_e {
        st_top, st_in_space, st_in_comment, st_in_string, st_lt, st_gt,
        st_literal, st_in_hexstring, st_inline_image, st_token_ready
    };

    class Members
    {
        friend class QPDFTokenizer;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&);
        void reset();

        // Lexer state
        state_e state;

        bool pound_special_in_name;
        bool allow_eof;
        bool include_ignorable;

        // Current token accumulation
        token_type_e type;
        std::string val;
        std::string raw_val;
        std::string error_message;
        bool unread_char;
        char char_to_unread;

        // State for strings
        int string_depth;
        bool string_ignoring_newline;
        char bs_num_register[4];
        bool last_char_was_bs;
        bool last_char_was_cr;
    };
    PointerHolder<Members> m;
};

#endif // QPDFTOKENIZER_HH

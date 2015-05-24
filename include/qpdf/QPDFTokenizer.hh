// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFTOKENIZER_HH__
#define __QPDFTOKENIZER_HH__

#include <qpdf/DLL.h>

#include <qpdf/InputSource.hh>
#include <qpdf/PointerHolder.hh>
#include <string>
#include <stdio.h>

class QPDFTokenizer
{
  public:
    // Token type tt_eof is only returned of allowEOF() is called on
    // the tokenizer.  tt_eof was introduced in QPDF version 4.1.
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
    };

    class Token
    {
      public:
	Token() : type(tt_bad) {}

	Token(token_type_e type, std::string const& value) :
	    type(type),
	    value(value)
	{
	}

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
	bool operator==(Token const& rhs)
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

    // Mode of operation:

    // Keep presenting characters and calling getToken() until
    // getToken() returns true.  When it does, be sure to check
    // unread_ch and to unread ch if it is true.

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

    // Read a token from an input source.  Context describes the
    // context in which the token is being read and is used in the
    // exception thrown if there is an error.
    QPDF_DLL
    Token readToken(PointerHolder<InputSource> input,
                    std::string const& context);

  private:
    void reset();
    void resolveLiteral();

    // Lexer state
    enum { st_top, st_in_comment, st_in_string, st_lt, st_gt,
	   st_literal, st_in_hexstring, st_token_ready } state;

    bool pound_special_in_name;
    bool allow_eof;

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
};

#endif // __QPDFTOKENIZER_HH__

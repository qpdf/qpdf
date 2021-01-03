// Copyright (c) 2005-2020 Jay Berkenbilt
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

#ifndef QUTIL_HH
#define QUTIL_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <qpdf/PointerHolder.hh>
#include <string>
#include <list>
#include <vector>
#include <stdexcept>
#include <functional>
#include <stdio.h>
#include <time.h>

class RandomDataProvider;

namespace QUtil
{
    // This is a collection of useful utility functions that don't
    // really go anywhere else.
    QPDF_DLL
    std::string int_to_string(long long, int length = 0);
    QPDF_DLL
    std::string uint_to_string(unsigned long long, int length = 0);
    QPDF_DLL
    std::string int_to_string_base(long long, int base, int length = 0);
    QPDF_DLL
    std::string uint_to_string_base(unsigned long long, int base, int length = 0);
    QPDF_DLL
    std::string double_to_string(double, int decimal_places = 0);

    // These string to number methods throw std::runtime_error on
    // underflow/overflow.
    QPDF_DLL
    long long string_to_ll(char const* str);
    QPDF_DLL
    int string_to_int(char const* str);
    QPDF_DLL
    unsigned long long string_to_ull(char const* str);
    QPDF_DLL
    unsigned int string_to_uint(char const* str);

    // Pipeline's write method wants unsigned char*, but we often have
    // some other type of string.  These methods do combinations of
    // const_cast and reinterpret_cast to give us an unsigned char*.
    // They should only be used when it is known that it is safe.
    // None of the pipelines in qpdf modify the data passed to them,
    // so within qpdf, it should always be safe.
    QPDF_DLL
    unsigned char* unsigned_char_pointer(std::string const& str);
    QPDF_DLL
    unsigned char* unsigned_char_pointer(char const* str);

    // Throw QPDFSystemError, which is derived from
    // std::runtime_error, with a string formed by appending to
    // "description: " the standard string corresponding to the
    // current value of errno. You can retrieve the value of errno by
    // calling getErrno() on the QPDFSystemError. Prior to qpdf 8.2.0,
    // this method threw system::runtime_error directly, but since
    // QPDFSystemError is derived from system::runtime_error, old code
    // that specifically catches std::runtime_error will still work.
    QPDF_DLL
    void throw_system_error(std::string const& description);

    // The status argument is assumed to be the return value of a
    // standard library call that sets errno when it fails.  If status
    // is -1, convert the current value of errno to a
    // std::runtime_error that includes the standard error string.
    // Otherwise, return status.
    QPDF_DLL
    int os_wrapper(std::string const& description, int status);

    QPDF_DLL
    bool with_mmapped_file(
        char const* filename,
        std::function<void(char const* buf, size_t len)> action);
    QPDF_DLL
    bool with_mmapped_file(
        FILE* f,
        std::function<void(char const* buf, size_t len)> action);

    // If the open fails, throws std::runtime_error. Otherwise, the
    // FILE* is returned. The filename should be UTF-8 encoded, even
    // on Windows. It will be converted as needed on Windows.
    QPDF_DLL
    FILE* safe_fopen(char const* filename, char const* mode);

    // The FILE* argument is assumed to be the return of fopen.  If
    // null, throw std::runtime_error.  Otherwise, return the FILE*
    // argument.
    QPDF_DLL
    FILE* fopen_wrapper(std::string const&, FILE*);

    // Wrap around off_t versions of fseek and ftell if available
    QPDF_DLL
    int seek(FILE* stream, qpdf_offset_t offset, int whence);
    QPDF_DLL
    qpdf_offset_t tell(FILE* stream);

    QPDF_DLL
    bool same_file(char const* name1, char const* name2);

    QPDF_DLL
    void remove_file(char const* path);

    // rename_file will overwrite newname if it exists
    QPDF_DLL
    void rename_file(char const* oldname, char const* newname);

    QPDF_DLL
    char* copy_string(std::string const&);

    // Returns lower-case hex-encoded version of the string, treating
    // each character in the input string as unsigned.  The output
    // string will be twice as long as the input string.
    QPDF_DLL
    std::string hex_encode(std::string const&);

    // Returns a string that is the result of decoding the input
    // string. The input string may consist of mixed case hexadecimal
    // digits. Any characters that are not hexadecimal digits will be
    // silently ignored. If there are an odd number of hexadecimal
    // digits, a trailing 0 will be assumed.
    QPDF_DLL
    std::string hex_decode(std::string const&);

    // Set stdin, stdout to binary mode
    QPDF_DLL
    void binary_stdout();
    QPDF_DLL
    void binary_stdin();
    // Set stdout to line buffered
    QPDF_DLL
    void setLineBuf(FILE*);


    // May modify argv0
    QPDF_DLL
    char* getWhoami(char* argv0);

    // Get the value of an environment variable in a portable fashion.
    // Returns true iff the variable is defined.  If `value' is
    // non-null, initializes it with the value of the variable.
    QPDF_DLL
    bool get_env(std::string const& var, std::string* value = 0);

    QPDF_DLL
    time_t get_current_time();

    // Return a string containing the byte representation of the UTF-8
    // encoding for the unicode value passed in.
    QPDF_DLL
    std::string toUTF8(unsigned long uval);

    // Return a string containing the byte representation of the
    // UTF-16 big-endian encoding for the unicode value passed in.
    // Unrepresentable code points are converted to U+FFFD.
    QPDF_DLL
    std::string toUTF16(unsigned long uval);

    // Test whether this is a UTF-16 big-endian string. This is
    // indicated by first two bytes being 0xFE 0xFF.
    QPDF_DLL
    bool is_utf16(std::string const&);

    // Convert a UTF-8 encoded string to UTF-16 big-endian.
    // Unrepresentable code points are converted to U+FFFD.
    QPDF_DLL
    std::string utf8_to_utf16(std::string const& utf8);

    // Convert a UTF-8 encoded string to the specified single-byte
    // encoding system by replacing all unsupported characters with
    // the given unknown_char.
    QPDF_DLL
    std::string utf8_to_ascii(
        std::string const& utf8, char unknown_char = '?');
    QPDF_DLL
    std::string utf8_to_win_ansi(
        std::string const& utf8, char unknown_char = '?');
    QPDF_DLL
    std::string utf8_to_mac_roman(
        std::string const& utf8, char unknown_char = '?');
    QPDF_DLL
    std::string utf8_to_pdf_doc(
        std::string const& utf8, char unknown_char = '?');

    // These versions return true if the conversion was successful and
    // false if any unrepresentable characters were found and had to
    // be substituted with the unknown character.
    QPDF_DLL
    bool utf8_to_ascii(
        std::string const& utf8, std::string& ascii, char unknown_char = '?');
    QPDF_DLL
    bool utf8_to_win_ansi(
        std::string const& utf8, std::string& win, char unknown_char = '?');
    QPDF_DLL
    bool utf8_to_mac_roman(
        std::string const& utf8, std::string& mac, char unknown_char = '?');
    QPDF_DLL
    bool utf8_to_pdf_doc(
        std::string const& utf8, std::string& pdfdoc, char unknown_char = '?');

    // Convert a UTF-16 big-endian encoded string to UTF-8.
    // Unrepresentable code points are converted to U+FFFD.
    QPDF_DLL
    std::string utf16_to_utf8(std::string const& utf16);

    // Convert from the specified single-byte encoding system to
    // UTF-8. There is no ascii_to_utf8 because all ASCII strings are
    // already valid UTF-8.
    QPDF_DLL
    std::string win_ansi_to_utf8(std::string const& win);
    QPDF_DLL
    std::string mac_roman_to_utf8(std::string const& mac);
    QPDF_DLL
    std::string pdf_doc_to_utf8(std::string const& pdfdoc);

    // Analyze a string for encoding. We can't tell the difference
    // between any single-byte encodings, and we can't tell for sure
    // whether a string that happens to be valid UTF-8 isn't a
    // different encoding, but we can at least tell a few things to
    // help us guess. If there are no characters with the high bit
    // set, has_8bit_chars is false, and the other values are also
    // false, even though ASCII strings are valid UTF-8. is_valid_utf8
    // means that the string is non-trivially valid UTF-8.
    QPDF_DLL
    void analyze_encoding(std::string const& str,
                          bool& has_8bit_chars,
                          bool& is_valid_utf8,
                          bool& is_utf16);

    // Try to compensate for previously incorrectly encoded strings.
    // We want to compensate for the following errors:
    //
    // * The string was supposed to be UTF-8 but was one of the
    //   single-byte encodings
    // * The string was supposed to be PDF Doc but was either UTF-8 or
    //   one of the other single-byte encodings
    //
    // The returned vector always contains the original string first,
    // and then it contains what the correct string would be in the
    // event that the original string was the result of any of the
    // above errors.
    //
    // This method is useful for attempting to recover a password that
    // may have been previously incorrectly encoded. For example, the
    // password was supposed to be UTF-8 but the previous application
    // used a password encoded in WinAnsi, or if the previous password
    // was supposed to be PDFDoc but was actually given as UTF-8 or
    // WinAnsi, this method would find the correct password.
    QPDF_DLL
    std::vector<std::string> possible_repaired_encodings(std::string);

    // Return a cryptographically secure random number.
    QPDF_DLL
    long random();

    // Initialize a buffer with cryptographically secure random bytes.
    QPDF_DLL
    void initializeWithRandomBytes(unsigned char* data, size_t len);

    // Supply a random data provider. Starting in qpdf 10.0.0, qpdf
    // uses the crypto provider as its source of random numbers. If
    // you are using the native crypto provider, then qpdf will either
    // use the operating system's secure random number source or, only
    // if enabled at build time, an insecure random source from
    // stdlib. The caller is responsible for managing the memory for
    // the RandomDataProvider. This method modifies a static variable.
    // If you are providing your own random data provider, you should
    // call this at the beginning of your program before creating any
    // QPDF objects. Passing a null to this method will reset the
    // library back to its default random data provider.
    QPDF_DLL
    void setRandomDataProvider(RandomDataProvider*);

    // This returns the random data provider that would be used the
    // next time qpdf needs random data.  It will never return null.
    // If no random data provider has been provided and the library
    // was not compiled with any random data provider available, an
    // exception will be thrown.
    QPDF_DLL
    RandomDataProvider* getRandomDataProvider();

    // Filename is UTF-8 encoded, even on Windows, as described in the
    // comments for safe_fopen.
    QPDF_DLL
    std::list<std::string> read_lines_from_file(
        char const* filename, bool preserve_eol = false);
    QPDF_DLL
    std::list<std::string> read_lines_from_file(
        std::istream&, bool preserve_eol = false);
    QPDF_DLL
    std::list<std::string> read_lines_from_file(
        FILE*, bool preserve_eol = false);
    QPDF_DLL
    void read_lines_from_file(
        std::function<bool(char&)> next_char,
        std::list<std::string>& lines,
        bool preserve_eol = false);

    QPDF_DLL
    void read_file_into_memory(
        char const* filename, PointerHolder<char>& file_buf, size_t& size);

    // This used to be called strcasecmp, but that is a macro on some
    // platforms, so we have to give it a name that is not likely to
    // be a macro anywhere.
    QPDF_DLL
    int str_compare_nocase(char const *, char const *);

    // These routines help the tokenizer recognize certain character
    // classes without using ctype, which we avoid because of locale
    // considerations.
    QPDF_DLL
    bool is_hex_digit(char);

    QPDF_DLL
    bool is_space(char);

    QPDF_DLL
    bool is_digit(char);

    QPDF_DLL
    bool is_number(char const*);

    // This method parses the numeric range syntax used by the qpdf
    // command-line tool. May throw std::runtime_error.
    QPDF_DLL
    std::vector<int> parse_numrange(char const* range, int max);

#ifndef QPDF_NO_WCHAR_T
    // If you are building qpdf on a stripped down system that doesn't
    // have wchar_t, such as may be the case in some embedded
    // environments, you may define QPDF_NO_WCHAR_T in your build.
    // This symbol is never defined automatically. Search for wchar_t
    // in qpdf's top-level README.md file for details.

    // Take an argv array consisting of wchar_t, as when wmain is
    // invoked, convert all UTF-16 encoded strings to UTF-8, and call
    // another main.
    QPDF_DLL
    int call_main_from_wmain(int argc, wchar_t* argv[],
                             std::function<int(int, char*[])> realmain);
#endif // QPDF_NO_WCHAR_T
};

#endif // QUTIL_HH

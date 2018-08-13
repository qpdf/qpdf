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

#ifndef QUTIL_HH
#define QUTIL_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <string>
#include <list>
#include <stdexcept>
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
    std::string int_to_string_base(long long, int base, int length = 0);
    QPDF_DLL
    std::string double_to_string(double, int decimal_places = 0);

    // These string to number methods throw std::runtime_error on
    // underflow/overflow.
    QPDF_DLL
    long long string_to_ll(char const* str);
    QPDF_DLL
    int string_to_int(char const* str);

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

    // If the open fails, throws std::runtime_error.  Otherwise, the
    // FILE* is returned.
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
    // UTF-16 BE encoding for the unicode value passed in.
    // Unrepresentable code points are converted to U+FFFD.
    QPDF_DLL
    std::string toUTF16(unsigned long uval);

    // If secure random number generation is supported on your
    // platform and qpdf was not compiled with insecure random number
    // generation, this returns a cryptographically secure random
    // number.  Otherwise it falls back to random from stdlib and
    // calls srandom automatically the first time it is called.
    QPDF_DLL
    long random();

    // Wrapper around srandom from stdlib.  Seeds the standard library
    // weak random number generator, which is not used if secure
    // random number generation is being used.  You never need to call
    // this method as it is called automatically if needed.
    QPDF_DLL
    void srandom(unsigned int seed);

    // Initialize a buffer with random bytes.  By default, qpdf tries
    // to use a secure random number source.  It can be configured at
    // compile time to use an insecure random number source (from
    // stdlib).  You can also call setRandomDataProvider with a
    // RandomDataProvider, in which case this method will get its
    // random bytes from that.

    QPDF_DLL
    void initializeWithRandomBytes(unsigned char* data, size_t len);

    // Supply a random data provider.  If not supplied, depending on
    // compile time options, qpdf will either use the operating
    // system's secure random number source or an insecure random
    // source from stdlib.  The caller is responsible for managing the
    // memory for the RandomDataProvider.  This method modifies a
    // static variable.  If you are providing your own random data
    // provider, you should call this at the beginning of your program
    // before creating any QPDF objects.  Passing a null to this
    // method will reset the library back to whichever of the built-in
    // random data handlers is appropriate based on how qpdf was
    // compiled.
    QPDF_DLL
    void setRandomDataProvider(RandomDataProvider*);

    // This returns the random data provider that would be used the
    // next time qpdf needs random data.  It will never return null.
    // If no random data provider has been provided and the library
    // was not compiled with any random data provider available, an
    // exception will be thrown.
    QPDF_DLL
    RandomDataProvider* getRandomDataProvider();

    QPDF_DLL
    std::list<std::string> read_lines_from_file(char const* filename);
    QPDF_DLL
    std::list<std::string> read_lines_from_file(std::istream&);

    QPDF_DLL
    int strcasecmp(char const *, char const *);

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
};

#endif // QUTIL_HH

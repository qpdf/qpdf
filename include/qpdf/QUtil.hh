// Copyright (c) 2005-2013 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QUTIL_HH__
#define __QUTIL_HH__

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <string>
#include <list>
#include <stdexcept>
#include <stdio.h>

namespace QUtil
{
    // This is a collection of useful utility functions that don't
    // really go anywhere else.
    QPDF_DLL
    std::string int_to_string(long long, int length = 0);
    QPDF_DLL
    std::string double_to_string(double, int decimal_places = 0);

    QPDF_DLL
    long long string_to_ll(char const* str);

    // Throw std::runtime_error with a string formed by appending to
    // "description: " the standard string corresponding to the
    // current value of errno.
    QPDF_DLL
    void throw_system_error(std::string const& description);

    // The status argument is assumed to be the return value of a
    // standard library call that sets errno when it fails.  If status
    // is -1, convert the current value of errno to a
    // std::runtime_error that includes the standard error string.
    // Otherwise, return status.
    QPDF_DLL
    int os_wrapper(std::string const& description, int status);

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
    char* copy_string(std::string const&);

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

    // Wrapper around random from stdlib.  Calls srandom automatically
    // the first time it is called.
    QPDF_DLL
    long random();

    // Wrapper around srandom from stdlib.
    QPDF_DLL
    void srandom(unsigned int seed);

    QPDF_DLL
    void initializeWithRandomBytes(unsigned char* data, size_t len);
};

#endif // __QUTIL_HH__

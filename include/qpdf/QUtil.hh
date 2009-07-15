// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QUTIL_HH__
#define __QUTIL_HH__

#include <string>
#include <list>
#include <stdio.h>
#include <sys/stat.h>

#include <qpdf/QEXC.hh>

namespace QUtil
{
    // This is a collection of useful utility functions that don't
    // really go anywhere else.
    std::string int_to_string(int, int length = 0);
    std::string double_to_string(double, int decimal_places = 0);

    // If status is -1, convert the current value of errno to a
    // QEXC::System exception.  Otherwise, return status.
    int os_wrapper(std::string const& description, int status)
	throw (QEXC::System);

    FILE* fopen_wrapper(std::string const&, FILE*)
	throw (QEXC::System);

    char* copy_string(std::string const&);

    // Set stdin, stdout to binary mode
    void binary_stdout();
    void binary_stdin();

    // May modify argv0
    char* getWhoami(char* argv0);

    // Get the value of an environment variable in a portable fashion.
    // Returns true iff the variable is defined.  If `value' is
    // non-null, initializes it with the value of the variable.
    bool get_env(std::string const& var, std::string* value = 0);

    time_t get_current_time();

    // Return a string containing the byte representation of the UTF-8
    // encoding for the unicode value passed in.
    std::string toUTF8(unsigned long uval);
};

#endif // __QUTIL_HH__

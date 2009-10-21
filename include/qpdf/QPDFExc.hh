// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFEXC_HH__
#define __QPDFEXC_HH__

#include <qpdf/DLL.h>
#include <qpdf/Constants.h>
#include <stdexcept>

class QPDFExc: public std::runtime_error
{
  public:
    DLL_EXPORT
    QPDFExc(qpdf_error_code_e error_code,
	    std::string const& filename,
	    std::string const& object,
	    off_t offset,
	    std::string const& message);
    DLL_EXPORT
    virtual ~QPDFExc() throw ();

    // To get a complete error string, call what(), provided by
    // std::exception.  The accessors below return the original values
    // used to create the exception.  Only the error code and message
    // are guaranteed to have non-zero/empty values.

    // There is no lookup code that maps numeric error codes into
    // strings.  The numeric error code is just another way to get at
    // the underlying issue, but it is more programmer-friendly than
    // trying to parse a string that is subject to change.

    DLL_EXPORT
    qpdf_error_code_e getErrorCode() const;
    DLL_EXPORT
    std::string const& getFilename() const;
    DLL_EXPORT
    std::string const& getObject() const;
    DLL_EXPORT
    off_t getFilePosition() const;
    DLL_EXPORT
    std::string const& getMessageDetail() const;

  private:
    static std::string createWhat(std::string const& filename,
				  std::string const& object,
				  off_t offset,
				  std::string const& message);

    qpdf_error_code_e error_code;
    std::string filename;
    std::string object;
    off_t offset;
    std::string message;
};

#endif // __QPDFEXC_HH__

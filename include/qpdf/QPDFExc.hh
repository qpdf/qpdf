// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFEXC_HH__
#define __QPDFEXC_HH__

#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <qpdf/Constants.h>
#include <string>
#include <stdexcept>

class QPDFExc: public std::runtime_error
{
  public:
    QPDF_DLL
    QPDFExc(qpdf_error_code_e error_code,
	    std::string const& filename,
	    std::string const& object,
	    qpdf_offset_t offset,
	    std::string const& message);
    QPDF_DLL
    virtual ~QPDFExc() throw ();

    // To get a complete error string, call what(), provided by
    // std::exception.  The accessors below return the original values
    // used to create the exception.  Only the error code and message
    // are guaranteed to have non-zero/empty values.

    // There is no lookup code that maps numeric error codes into
    // strings.  The numeric error code is just another way to get at
    // the underlying issue, but it is more programmer-friendly than
    // trying to parse a string that is subject to change.

    QPDF_DLL
    qpdf_error_code_e getErrorCode() const;
    QPDF_DLL
    std::string const& getFilename() const;
    QPDF_DLL
    std::string const& getObject() const;
    QPDF_DLL
    qpdf_offset_t getFilePosition() const;
    QPDF_DLL
    std::string const& getMessageDetail() const;

  private:
    static std::string createWhat(std::string const& filename,
				  std::string const& object,
				  qpdf_offset_t offset,
				  std::string const& message);

    qpdf_error_code_e error_code;
    std::string filename;
    std::string object;
    qpdf_offset_t offset;
    std::string message;
};

#endif // __QPDFEXC_HH__

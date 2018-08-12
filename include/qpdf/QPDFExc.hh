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

#ifndef QPDFEXC_HH
#define QPDFEXC_HH

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

#endif // QPDFEXC_HH

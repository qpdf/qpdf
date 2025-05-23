// Copyright (c) 2005-2021 Jay Berkenbilt
// Copyright (c) 2022-2025 Jay Berkenbilt and Manfred Holger
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under
// the License.
//
// Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic
// License. At your option, you may continue to consider qpdf to be licensed under those terms.
// Please see the manual for additional information.

#ifndef QPDFSYSTEMERROR_HH
#define QPDFSYSTEMERROR_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <stdexcept>
#include <string>

class QPDF_DLL_CLASS QPDFSystemError: public std::runtime_error
{
  public:
    QPDF_DLL
    QPDFSystemError(std::string const& description, int system_errno);

    ~QPDFSystemError() noexcept override = default;

    // To get a complete error string, call what(), provided by std::exception.  The accessors below
    // return the original values used to create the exception.

    QPDF_DLL
    std::string const& getDescription() const;
    QPDF_DLL
    int getErrno() const;

  private:
    QPDF_DLL_PRIVATE
    static std::string createWhat(std::string const& description, int system_errno);

    // This class does not use the Members pattern to avoid needless memory allocations during
    // exception handling.

    std::string description;
    int system_errno;
};

#endif // QPDFSYSTEMERROR_HH

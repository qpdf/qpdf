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

#ifndef PDFVERSION_HH
#define PDFVERSION_HH

#include <qpdf/DLL.h>
#include <string>

// Represent a PDF version. PDF versions are typically major.minor, but PDF 1.7 has several
// extension levels as the ISO 32000 spec was in progress. This class helps with comparison of
// versions.
class PDFVersion
{
  public:
    PDFVersion() = default;
    PDFVersion(PDFVersion const&) = default;
    PDFVersion& operator=(PDFVersion const&) = default;

    QPDF_DLL
    PDFVersion(int major, int minor, int extension = 0);
    QPDF_DLL
    bool operator<(PDFVersion const& rhs) const;
    QPDF_DLL
    bool operator==(PDFVersion const& rhs) const;

    // Replace this version with the other one if the other one is greater.
    QPDF_DLL
    void updateIfGreater(PDFVersion const& other);

    // Initialize a string and integer suitable for passing to QPDFWriter::setMinimumPDFVersion or
    // QPDFWriter::forcePDFVersion.
    QPDF_DLL
    void getVersion(std::string& version, int& extension_level) const;

    QPDF_DLL
    int getMajor() const;
    QPDF_DLL
    int getMinor() const;
    QPDF_DLL
    int getExtensionLevel() const;

  private:
    int major_version{0};
    int minor_version{0};
    int extension_level{0};
};

#endif // PDFVERSION_HH

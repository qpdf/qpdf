// Copyright (c) 2005-2022 Jay Berkenbilt
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

#ifndef QPDFEMBEDDEDFILEDOCUMENTHELPER_HH
#define QPDFEMBEDDEDFILEDOCUMENTHELPER_HH

#include <qpdf/QPDFDocumentHelper.hh>

#include <qpdf/DLL.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFFileSpecObjectHelper.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>

#include <map>
#include <memory>

// This class provides a higher level interface around document-level
// file attachments, also known as embedded files. These are discussed
// in sections 7.7.4 and 7.11 of the ISO-32000 PDF specification.

class QPDFEmbeddedFileDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFEmbeddedFileDocumentHelper(QPDF&);
    QPDF_DLL
    virtual ~QPDFEmbeddedFileDocumentHelper() = default;

    QPDF_DLL
    bool hasEmbeddedFiles() const;

    QPDF_DLL
    std::map<std::string, std::shared_ptr<QPDFFileSpecObjectHelper>>
    getEmbeddedFiles();

    // If an embedded file with the given name exists, return a
    // (shared) pointer to it. Otherwise, return nullptr.
    QPDF_DLL
    std::shared_ptr<QPDFFileSpecObjectHelper>
    getEmbeddedFile(std::string const& name);

    // Add or replace an attachment
    QPDF_DLL
    void replaceEmbeddedFile(
        std::string const& name, QPDFFileSpecObjectHelper const&);

    // Remove an embedded file if present. Return value is true if the
    // file was present and was removed. This method not only removes
    // the embedded file from the embedded files name tree but also
    // nulls out the file specification dictionary. This means that
    // any references to this file from file attachment annotations
    // will also stop working. This is the best way to make the
    // attachment actually disappear from the file and not just from
    // the list of attachments.
    QPDF_DLL
    bool removeEmbeddedFile(std::string const& name);

  private:
    void initEmbeddedFiles();

    class Members
    {
        friend class QPDFEmbeddedFileDocumentHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;

        std::shared_ptr<QPDFNameTreeObjectHelper> embedded_files;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFEMBEDDEDFILEDOCUMENTHELPER_HH

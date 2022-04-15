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

#ifndef QPDFFILESPECOBJECTHELPER_HH
#define QPDFFILESPECOBJECTHELPER_HH

#include <qpdf/QPDFObjectHelper.hh>

#include <qpdf/DLL.h>

#include <qpdf/QPDFEFStreamObjectHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>

// This class provides a higher level interface around File
// Specification dictionaries, which are discussed in section 7.11 of
// the ISO-32000 PDF specification.

class QPDFFileSpecObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFFileSpecObjectHelper(QPDFObjectHandle);
    QPDF_DLL
    virtual ~QPDFFileSpecObjectHelper() = default;

    QPDF_DLL
    std::string getDescription();

    // Get the main filename for this file specification. In priority
    // order, check /UF, /F, /Unix, /DOS, /Mac.
    QPDF_DLL
    std::string getFilename();

    // Return any of /UF, /F, /Unix, /DOS, /Mac filename keys that may
    // be present in the object.
    QPDF_DLL
    std::map<std::string, std::string> getFilenames();

    // Get the requested embedded file stream for this file
    // specification. If key is empty, In priority order, check /UF,
    // /F, /Unix, /DOS, /Mac. Returns a null object if not found. If
    // this is an actual embedded file stream, its data is the content
    // of the attachment. You can also use
    // QPDFEFStreamObjectHelper for higher level access to
    // the parameters.
    QPDF_DLL
    QPDFObjectHandle getEmbeddedFileStream(std::string const& key = "");

    // Return the /EF key of the file spec, which is a map from file
    // name key to embedded file stream.
    QPDF_DLL
    QPDFObjectHandle getEmbeddedFileStreams();

    // Setters return a reference to this object so that they can be
    // used as fluent interfaces, e.g.
    // fsoh.setDescription(x).setFilename(y);

    // Create a new filespec as an indirect object with the given
    // filename, and attach the contents of the specified file as data
    // in an embedded file stream.
    QPDF_DLL
    static QPDFFileSpecObjectHelper createFileSpec(
        QPDF& qpdf, std::string const& filename, std::string const& fullpath);

    // Create a new filespec as an indirect object with the given
    // unicode filename and embedded file stream. The file name will
    // be used as both /UF and /F. If you need to override, call
    // setFilename.
    QPDF_DLL
    static QPDFFileSpecObjectHelper createFileSpec(
        QPDF& qpdf, std::string const& filename, QPDFEFStreamObjectHelper);

    QPDF_DLL
    QPDFFileSpecObjectHelper& setDescription(std::string const&);
    // setFilename sets /UF to unicode_name. If compat_name is empty,
    // it is also set to unicode_name. unicode_name should be a UTF-8
    // encoded string. compat_name is converted to a string
    // QPDFObjectHandle literally, preserving whatever encoding it
    // might happen to have.
    QPDF_DLL
    QPDFFileSpecObjectHelper& setFilename(
        std::string const& unicode_name, std::string const& compat_name = "");

  private:
    class Members
    {
        friend class QPDFFileSpecObjectHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFFILESPECOBJECTHELPER_HH

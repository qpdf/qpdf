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

#ifndef QPDFEFSTREAMOBJECTHELPER_HH
#define QPDFEFSTREAMOBJECTHELPER_HH

#include <qpdf/QPDFObjectHelper.hh>

#include <qpdf/DLL.h>

#include <qpdf/QPDFObjectHandle.hh>
#include <functional>

// This class provides a higher level interface around Embedded File
// Streams, which are discussed in section 7.11.4 of the ISO-32000 PDF
// specification.

class QPDFEFStreamObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFEFStreamObjectHelper(QPDFObjectHandle);
    QPDF_DLL
    virtual ~QPDFEFStreamObjectHelper() = default;

    // Date parameters are strings that conform to the PDF spec for
    // date/time strings, which is "D:yyyymmddhhmmss<z>" where <z> is
    // either "Z" for UTC or "-hh'mm'" or "+hh'mm'" for timezone
    // offset. Examples: "D:20210207161528-05'00'",
    // "D:20210207211528Z". See QUtil::qpdf_time_to_pdf_time.

    QPDF_DLL
    std::string getCreationDate();
    QPDF_DLL
    std::string getModDate();
    // Get size as reported in the object; return 0 if not present.
    QPDF_DLL
    size_t getSize();
    // Subtype is a mime type such as "text/plain"
    QPDF_DLL
    std::string getSubtype();
    // Return the checksum as stored in the object as a binary string.
    // This does not check consistency with the data. If not present,
    // return an empty string. The PDF spec specifies this as an MD5
    // checksum and notes that it is not to be used for security
    // purposes since MD5 is known not to be secure.
    QPDF_DLL
    std::string getChecksum();

    // Setters return a reference to this object so that they can be
    // used as fluent interfaces, e.g.
    // efsoh.setCreationDate(x).setModDate(y);

    // Create a new embedded file stream with the given stream data,
    // which can be provided in any of several ways. To get the new
    // object back, call getObjectHandle() on the returned object. The
    // checksum and size are computed automatically and stored. Other
    // parameters may be supplied using setters defined below.
    QPDF_DLL
    static QPDFEFStreamObjectHelper
    createEFStream(QPDF& qpdf, std::shared_ptr<Buffer> data);
    QPDF_DLL
    static QPDFEFStreamObjectHelper
    createEFStream(QPDF& qpdf, std::string const& data);
    // The provider function must write the data to the given
    // pipeline. The function may be called multiple times by the qpdf
    // library. You can pass QUtil::file_provider(filename) as the
    // provider to have the qpdf library provide the contents of
    // filename as a binary.
    QPDF_DLL
    static QPDFEFStreamObjectHelper
    createEFStream(QPDF& qpdf, std::function<void(Pipeline*)> provider);

    // Setters for other parameters
    QPDF_DLL
    QPDFEFStreamObjectHelper& setCreationDate(std::string const&);
    QPDF_DLL
    QPDFEFStreamObjectHelper& setModDate(std::string const&);

    // Set subtype as a mime-type, e.g. "text/plain" or
    // "application/pdf".
    QPDF_DLL
    QPDFEFStreamObjectHelper& setSubtype(std::string const&);

  private:
    QPDFObjectHandle getParam(std::string const& pkey);
    void setParam(std::string const& pkey, QPDFObjectHandle const&);
    static QPDFEFStreamObjectHelper newFromStream(QPDFObjectHandle stream);

    class Members
    {
        friend class QPDFEFStreamObjectHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFEFSTREAMOBJECTHELPER_HH

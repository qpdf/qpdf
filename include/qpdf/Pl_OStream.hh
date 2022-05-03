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

// End-of-line pipeline that simply writes its data to a stdio FILE* object.

#ifndef PL_OSTREAM_HH
#define PL_OSTREAM_HH

#include <qpdf/Pipeline.hh>

#include <iostream>

//
// This pipeline is reusable.
//

class QPDF_DLL_CLASS Pl_OStream: public Pipeline
{
  public:
    // os is externally maintained; this class just writes to and
    // flushes it.  It does not close it.
    QPDF_DLL
    Pl_OStream(char const* identifier, std::ostream& os);
    QPDF_DLL
    virtual ~Pl_OStream();

    QPDF_DLL
    virtual void write(unsigned char const* buf, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_OStream;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(std::ostream&);
        Members(Members const&) = delete;

        std::ostream& os;
    };

    std::shared_ptr<Members> m;
};

#endif // PL_OSTREAM_HH

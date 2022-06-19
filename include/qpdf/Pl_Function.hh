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

#ifndef PL_FUNCTION_HH
#define PL_FUNCTION_HH

// This pipeline calls an arbitrary function with whatever data is
// passed to it. This pipeline can be reused.
//
// For this pipeline, "next" may be null. If a next pointer is
// provided, this pipeline will also pass the data through to it and
// will forward finish() to it.
//
// It is okay to not call finish() on this pipeline if it has no
// "next".

#include <qpdf/Pipeline.hh>

#include <functional>

class QPDF_DLL_CLASS Pl_Function: public Pipeline
{
  public:
    typedef std::function<void(unsigned char const*, size_t)> writer_t;

    QPDF_DLL
    Pl_Function(char const* identifier, Pipeline* next, writer_t fn);
    QPDF_DLL
    virtual ~Pl_Function();

    QPDF_DLL
    virtual void write(unsigned char const* buf, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_Function;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(writer_t);
        Members(Members const&) = delete;

        writer_t fn;
    };

    std::shared_ptr<Members> m;
};

#endif // PL_FUNCTION_HH

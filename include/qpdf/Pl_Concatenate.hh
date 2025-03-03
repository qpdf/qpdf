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

#ifndef PL_CONCATENATE_HH
#define PL_CONCATENATE_HH

#include <qpdf/Pipeline.hh>

// This pipeline will drop all regular finish calls rather than passing them onto next.  To finish
// downstream streams, call manualFinish.  This makes it possible to pipe multiple streams (e.g.
// with QPDFObjectHandle::pipeStreamData) to a downstream like Pl_Flate that can't handle multiple
// calls to finish().
class QPDF_DLL_CLASS Pl_Concatenate: public Pipeline
{
  public:
    QPDF_DLL
    Pl_Concatenate(char const* identifier, Pipeline* next);

    QPDF_DLL
    ~Pl_Concatenate() override;

    QPDF_DLL
    void write(unsigned char const* data, size_t len) override;

    QPDF_DLL
    void finish() override;

    // At the very end, call manualFinish to actually finish the rest of the pipeline.
    QPDF_DLL
    void manualFinish();

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_Concatenate;

      public:
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::unique_ptr<Members> m{nullptr};
};

#endif // PL_CONCATENATE_HH

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

#ifndef PL_STRING_HH
#define PL_STRING_HH

#include <qpdf/Pipeline.hh>

#include <string>

// This pipeline accumulates the data passed to it into a std::string, a reference to which is
// passed in at construction. Each subsequent use of this pipeline appends to the data accumulated
// so far.
//
// For this pipeline, "next" may be null. If a next pointer is provided, this pipeline will also
// pass the data through to it and will forward finish() to it.
//
// It is okay to not call finish() on this pipeline if it has no "next". This makes it easy to stick
// this in front of another pipeline to capture data that is written to the other pipeline without
// interfering with when finish is called on the other pipeline and without having to put a
// Pl_Concatenate after it.
class QPDF_DLL_CLASS Pl_String: public Pipeline
{
  public:
    QPDF_DLL
    Pl_String(char const* identifier, Pipeline* next, std::string& s);
    QPDF_DLL
    ~Pl_String() override;

    QPDF_DLL
    void write(unsigned char const* buf, size_t len) override;
    QPDF_DLL
    void finish() override;

  private:
    class Members;

    std::unique_ptr<Members> m;
};

#endif // PL_STRING_HH

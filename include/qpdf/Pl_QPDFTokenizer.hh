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

#ifndef PL_QPDFTOKENIZER_HH
#define PL_QPDFTOKENIZER_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/PointerHolder.hh> // unused -- remove in qpdf 12 (see #785)
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFTokenizer.hh>

#include <memory>

// Tokenize the incoming text using QPDFTokenizer and pass the tokens
// in turn to a QPDFObjectHandle::TokenFilter object. All bytes of
// incoming content will be included in exactly one token and passed
// downstream.

// This is a very low-level interface for working with token filters.
// Most code will want to use QPDFObjectHandle::filterPageContents or
// QPDFObjectHandle::addTokenFilter. See QPDFObjectHandle.hh for
// details.

class QPDF_DLL_CLASS Pl_QPDFTokenizer: public Pipeline
{
  public:
    // Whatever pipeline is provided as "next" will be set as the
    // pipeline that the token filter writes to. If next is not
    // provided, any output written by the filter will be discarded.
    QPDF_DLL
    Pl_QPDFTokenizer(
        char const* identifier,
        QPDFObjectHandle::TokenFilter* filter,
        Pipeline* next = 0);
    QPDF_DLL
    virtual ~Pl_QPDFTokenizer();
    QPDF_DLL
    virtual void write(unsigned char const* buf, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_QPDFTokenizer;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members();
        Members(Members const&) = delete;

        QPDFObjectHandle::TokenFilter* filter;
        QPDFTokenizer tokenizer;
        Pl_Buffer buf;
    };
    std::shared_ptr<Members> m;
};

#endif // PL_QPDFTOKENIZER_HH

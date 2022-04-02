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

#ifndef QPDFSTREAMFILTER_HH
#define QPDFSTREAMFILTER_HH

#include <qpdf/DLL.h>
#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFObjectHandle.hh>

class QPDF_DLL_CLASS QPDFStreamFilter
{
  public:
    QPDF_DLL
    QPDFStreamFilter() = default;

    QPDF_DLL
    virtual ~QPDFStreamFilter() = default;

    // A QPDFStreamFilter class must implement, at a minimum,
    // setDecodeParms() and getDecodePipeline(). QPDF will always call
    // setDecodeParms() before calling getDecodePipeline(). It is
    // expected that you will store any needed information from
    // decode_parms (or the decode_parms object itself) in your
    // instance so that it can be used to construct the decode
    // pipeline.

    // Return a boolean indicating whether your filter can proceed
    // with the given /DecodeParms. The default implementation accepts
    // a null object and rejects everything else.
    QPDF_DLL
    virtual bool setDecodeParms(QPDFObjectHandle decode_parms);

    // Return a pipeline that will decode data encoded with your
    // filter. Your implementation must ensure that the pipeline is
    // deleted when the instance of your class is destroyed.
    QPDF_DLL
    virtual Pipeline* getDecodePipeline(Pipeline* next) = 0;

    // If your filter implements "specialized" compression or lossy
    // compression, override one or both of these methods. The default
    // implementations return false. See comments in QPDFWriter for
    // details. QPDF defines specialized compression as non-lossy
    // compression not intended for general-purpose data. qpdf, by
    // default, doesn't mess with streams that are compressed with
    // specialized compression, the idea being that the decision to
    // use that compression scheme would fall outside of what
    // QPDFWriter would know anything about, so any attempt to decode
    // and re-encode would probably be undesirable.
    QPDF_DLL
    virtual bool isSpecializedCompression();
    QPDF_DLL
    virtual bool isLossyCompression();

  private:
    QPDFStreamFilter(QPDFStreamFilter const&) = delete;
    QPDFStreamFilter& operator=(QPDFStreamFilter const&) = delete;
};

#endif // QPDFSTREAMFILTER_HH

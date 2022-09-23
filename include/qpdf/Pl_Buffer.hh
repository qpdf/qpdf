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

#ifndef PL_BUFFER_HH
#define PL_BUFFER_HH

// This pipeline accumulates the data passed to it into a memory
// buffer.  Each subsequent use of this buffer appends to the data
// accumulated so far.  getBuffer() may be called only after calling
// finish() and before calling any subsequent write().  At that point,
// a dynamically allocated Buffer object is returned and the internal
// buffer is reset.  The caller is responsible for deleting the
// returned Buffer.
//
// For this pipeline, "next" may be null.  If a next pointer is
// provided, this pipeline will also pass the data through to it.

#include <qpdf/Buffer.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/PointerHolder.hh> // unused -- remove in qpdf 12 (see #785)

#include <memory>

class QPDF_DLL_CLASS Pl_Buffer: public Pipeline
{
  public:
    QPDF_DLL
    Pl_Buffer(char const* identifier, Pipeline* next = 0);
    QPDF_DLL
    virtual ~Pl_Buffer();
    QPDF_DLL
    virtual void write(unsigned char const*, size_t);
    QPDF_DLL
    virtual void finish();

    // Each call to getBuffer() resets this object -- see notes above.
    // The caller is responsible for deleting the returned Buffer
    // object. See also getBufferSharedPointer() and getMallocBuffer().
    QPDF_DLL
    Buffer* getBuffer();

    // Same as getBuffer but wraps the result in a shared pointer.
    QPDF_DLL
    std::shared_ptr<Buffer> getBufferSharedPointer();

    // getMallocBuffer behaves in the same was as getBuffer except the
    // buffer is allocated with malloc(), making it suitable for use
    // when calling from other languages. If there is no data, *buf is
    // set to a null pointer and *len is set to 0. Otherwise, *buf is
    // a buffer of size *len allocated with malloc(). It is the
    // caller's responsibility to call free() on the buffer.
    QPDF_DLL
    void getMallocBuffer(unsigned char** buf, size_t* len);

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_Buffer;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;

        bool ready{true};
        std::basic_string<unsigned char> data;
    };

    std::shared_ptr<Members> m;
};

#endif // PL_BUFFER_HH

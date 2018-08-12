// Copyright (c) 2005-2018 Jay Berkenbilt
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

#include <qpdf/Pipeline.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <list>

class Pl_Buffer: public Pipeline
{
  public:
    QPDF_DLL
    Pl_Buffer(char const* identifier, Pipeline* next = 0);
    QPDF_DLL
    virtual ~Pl_Buffer();
    QPDF_DLL
    virtual void write(unsigned char*, size_t);
    QPDF_DLL
    virtual void finish();

    // Each call to getBuffer() resets this object -- see notes above.
    // The caller is responsible for deleting the returned Buffer
    // object.
    QPDF_DLL
    Buffer* getBuffer();

  private:
    bool ready;
    std::list<PointerHolder<Buffer> > data;
    size_t total_size;
};

#endif // PL_BUFFER_HH

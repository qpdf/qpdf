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

#ifndef BUFFER_HH
#define BUFFER_HH

#include <qpdf/DLL.h>

#include <cstddef>
#include <memory>
#include <string>

class Buffer
{
  public:
    QPDF_DLL
    Buffer();

    // Create a Buffer object whose memory is owned by the class and will be freed when the Buffer
    // object is destroyed.
    QPDF_DLL
    Buffer(size_t size);
    QPDF_DLL
    Buffer(std::string&& content);

    // Create a Buffer object whose memory is owned by the caller and will not be freed when the
    // Buffer is destroyed.
    QPDF_DLL
    Buffer(unsigned char* buf, size_t size);
    QPDF_DLL
    Buffer(std::string& content);

    Buffer(Buffer const&) = delete;
    Buffer& operator=(Buffer const&) = delete;

    QPDF_DLL
    Buffer(Buffer&&) noexcept;
    QPDF_DLL
    Buffer& operator=(Buffer&&) noexcept;
    QPDF_DLL
    ~Buffer();
    QPDF_DLL
    size_t getSize() const;
    QPDF_DLL
    unsigned char const* getBuffer() const;
    QPDF_DLL
    unsigned char* getBuffer();

    // Create a new copy of the Buffer. The new Buffer owns an independent copy of the data.
    QPDF_DLL
    Buffer copy() const;

  private:
    class Members;

    void copy(Buffer const&);

    std::unique_ptr<Members> m;
};

#endif // BUFFER_HH

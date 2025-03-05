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

#ifndef QPDF_BUFFERINPUTSOURCE_HH
#define QPDF_BUFFERINPUTSOURCE_HH

#include <qpdf/Buffer.hh>
#include <qpdf/InputSource.hh>

class QPDF_DLL_CLASS BufferInputSource: public InputSource
{
  public:
    // If own_memory is true, BufferInputSource will delete the buffer when finished with it.
    // Otherwise, the caller owns the memory.
    QPDF_DLL
    BufferInputSource(std::string const& description, Buffer* buf, bool own_memory = false);

    // NB This overload copies the string contents.
    QPDF_DLL
    BufferInputSource(std::string const& description, std::string const& contents);
    QPDF_DLL
    ~BufferInputSource() override;
    QPDF_DLL
    qpdf_offset_t findAndSkipNextEOL() override;
    QPDF_DLL
    std::string const& getName() const override;
    QPDF_DLL
    qpdf_offset_t tell() override;
    QPDF_DLL
    void seek(qpdf_offset_t offset, int whence) override;
    QPDF_DLL
    void rewind() override;
    QPDF_DLL
    size_t read(char* buffer, size_t length) override;
    QPDF_DLL
    void unreadCh(char ch) override;

  private:
    bool own_memory;
    std::string description;
    Buffer* buf;
    qpdf_offset_t cur_offset;
    qpdf_offset_t max_offset;
};

#endif // QPDF_BUFFERINPUTSOURCE_HH

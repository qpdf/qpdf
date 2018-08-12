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

#ifndef QPDF_BUFFERINPUTSOURCE_HH
#define QPDF_BUFFERINPUTSOURCE_HH

#include <qpdf/InputSource.hh>
#include <qpdf/Buffer.hh>

class BufferInputSource: public InputSource
{
  public:
    // If own_memory is true, BufferInputSource will delete the buffer
    // when finished with it. Otherwise, the caller owns the memory.
    QPDF_DLL
    BufferInputSource(std::string const& description, Buffer* buf,
                      bool own_memory = false);
    QPDF_DLL
    BufferInputSource(std::string const& description,
                      std::string const& contents);
    QPDF_DLL
    virtual ~BufferInputSource();
    QPDF_DLL
    virtual qpdf_offset_t findAndSkipNextEOL();
    QPDF_DLL
    virtual std::string const& getName() const;
    QPDF_DLL
    virtual qpdf_offset_t tell();
    QPDF_DLL
    virtual void seek(qpdf_offset_t offset, int whence);
    QPDF_DLL
    virtual void rewind();
    QPDF_DLL
    virtual size_t read(char* buffer, size_t length);
    QPDF_DLL
    virtual void unreadCh(char ch);

  private:
    bool own_memory;
    std::string description;
    Buffer* buf;
    qpdf_offset_t cur_offset;
};

#endif // QPDF_BUFFERINPUTSOURCE_HH

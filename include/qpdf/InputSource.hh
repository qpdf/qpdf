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

#ifndef QPDF_INPUTSOURCE_HH
#define QPDF_INPUTSOURCE_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <cstdio>
#include <memory>
#include <string>

// Remember to use QPDF_DLL_CLASS on anything derived from InputSource so it will work with
// dynamic_cast across the shared object boundary.
class QPDF_DLL_CLASS InputSource
{
  public:
    InputSource() = default;

    virtual ~InputSource() = default;

    class QPDF_DLL_CLASS Finder
    {
      public:
        QPDF_DLL
        Finder() = default;
        QPDF_DLL
        virtual ~Finder() = default;
        virtual bool check() = 0;
    };

    QPDF_DLL
    void setLastOffset(qpdf_offset_t);
    QPDF_DLL
    qpdf_offset_t getLastOffset() const;
    QPDF_DLL
    std::string readLine(size_t max_line_length);

    // Find first or last occurrence of a sequence of characters starting within the range defined
    // by offset and len such that, when the input source is positioned at the beginning of that
    // sequence, finder.check() returns true. If len is 0, the search proceeds until EOF. If a
    // qualifying pattern is found, these methods return true and leave the input source positioned
    // wherever check() left it at the end of the matching pattern.
    QPDF_DLL
    bool findFirst(char const* start_chars, qpdf_offset_t offset, size_t len, Finder& finder);
    QPDF_DLL
    bool findLast(char const* start_chars, qpdf_offset_t offset, size_t len, Finder& finder);

    virtual qpdf_offset_t findAndSkipNextEOL() = 0;
    virtual std::string const& getName() const = 0;
    virtual qpdf_offset_t tell() = 0;
    virtual void seek(qpdf_offset_t offset, int whence) = 0;
    virtual void rewind() = 0;
    virtual size_t read(char* buffer, size_t length) = 0;

    // Note: you can only unread the character you just read. The specific character is ignored by
    // some implementations, and the implementation doesn't check this. Use of unreadCh is
    // semantically equivalent to seek(-1, SEEK_CUR) but is much more efficient.
    virtual void unreadCh(char ch) = 0;

    // The following methods are for internal use by qpdf only.
    inline size_t read(std::string& str, size_t count, qpdf_offset_t at = -1);
    inline std::string read(size_t count, qpdf_offset_t at = -1);
    size_t read_line(std::string& str, size_t count, qpdf_offset_t at = -1);
    std::string read_line(size_t count, qpdf_offset_t at = -1);
    inline qpdf_offset_t fastTell();
    inline bool fastRead(char&);
    inline void fastUnread(bool);
    inline void loadBuffer();

  protected:
    qpdf_offset_t last_offset{0};

  private:
    // State for fast... methods
    static const qpdf_offset_t buf_size = 128;
    char buffer[buf_size];
    qpdf_offset_t buf_len = 0;
    qpdf_offset_t buf_idx = 0;
    qpdf_offset_t buf_start = 0;
};

#endif // QPDF_INPUTSOURCE_HH

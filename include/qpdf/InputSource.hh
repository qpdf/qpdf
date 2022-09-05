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

#ifndef QPDF_INPUTSOURCE_HH
#define QPDF_INPUTSOURCE_HH

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Types.h>

#include <memory>
#include <stdio.h>
#include <string>

// Remember to use QPDF_DLL_CLASS on anything derived from InputSource
// so it will work with dynamic_cast across the shared object
// boundary.
class QPDF_DLL_CLASS InputSource
{
  public:
    QPDF_DLL
    InputSource() :
        last_offset(0)
    {
    }
    QPDF_DLL
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

    // Find first or last occurrence of a sequence of characters
    // starting within the range defined by offset and len such that,
    // when the input source is positioned at the beginning of that
    // sequence, finder.check() returns true. If len is 0, the search
    // proceeds until EOF. If a qualifying pattern is found, these
    // methods return true and leave the input source positioned
    // wherever check() left it at the end of the matching pattern.
    QPDF_DLL
    bool findFirst(
        char const* start_chars,
        qpdf_offset_t offset,
        size_t len,
        Finder& finder);
    QPDF_DLL
    bool findLast(
        char const* start_chars,
        qpdf_offset_t offset,
        size_t len,
        Finder& finder);

    virtual qpdf_offset_t findAndSkipNextEOL() = 0;
    virtual std::string const& getName() const = 0;
    virtual qpdf_offset_t tell() = 0;
    virtual void seek(qpdf_offset_t offset, int whence) = 0;
    virtual void rewind() = 0;
    virtual size_t read(char* buffer, size_t length) = 0;

    // Note: you can only unread the character you just read. The
    // specific character is ignored by some implementations, and the
    // implementation doesn't check this. Use of unreadCh is
    // semantically equivalent to seek(-1, SEEK_CUR) but is much more
    // efficient.
    virtual void unreadCh(char ch) = 0;

    // The following methods are for use by QPDFTokenizer
    inline qpdf_offset_t fastTell();
    inline bool fastRead(char&);
    inline void fastUnread(bool);
    inline void loadCache();

  protected:
    qpdf_offset_t last_offset;

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class InputSource;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::shared_ptr<Members> m;

    // State for fast... methods
    static const qpdf_offset_t cache_size{128};
    char cache[cache_size];
    qpdf_offset_t cache_len{0};
    qpdf_offset_t cache_idx{0};
    qpdf_offset_t cache_start{0};
};

inline void
InputSource::loadCache()
{
    this->cache_idx = 0;
    this->cache_len = qpdf_offset_t(read(this->cache, this->cache_size));
    // NB read sets last_offset
    this->cache_start = this->last_offset;
}

inline qpdf_offset_t
InputSource::fastTell()
{
    if (this->cache_len == 0) {
        loadCache();
    } else {
        auto curr = tell();
        if (curr < this->cache_start ||
            curr >= (this->cache_start + this->cache_len)) {
            loadCache();
        } else {
            this->last_offset = curr;
            this->cache_idx = curr - this->cache_start;
        }
    }
    return this->last_offset;
}

inline bool
InputSource::fastRead(char& ch)
{
    // Before calling fastRead, fastTell must be called to prepare the cache.
    // Once reading is complete, fastUnread must be called to set the correct
    // file position.
    if (this->cache_idx < this->cache_len) {
        ch = this->cache[this->cache_idx];
        ++(this->cache_idx);
        ++(this->last_offset);
        return true;

    } else if (this->cache_len == 0) {
        return false;
    } else {
        seek(this->cache_start + this->cache_len, SEEK_SET);
        fastTell();
        return fastRead(ch);
    }
}

inline void
InputSource::fastUnread(bool back)
{
    this->last_offset -= back ? 1 : 0;
    seek(this->last_offset, SEEK_SET);
}

#endif // QPDF_INPUTSOURCE_HH

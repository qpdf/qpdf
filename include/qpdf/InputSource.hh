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

#ifndef QPDF_INPUTSOURCE_HH
#define QPDF_INPUTSOURCE_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <stdio.h>
#include <string>

class InputSource
{
  public:
    QPDF_DLL
    InputSource() :
        last_offset(0)
    {
    }
    QPDF_DLL
    virtual ~InputSource()
    {
    }

    class Finder
    {
      public:
        Finder()
        {
        }
        virtual ~Finder()
        {
        }

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
    bool findFirst(char const* start_chars,
                   qpdf_offset_t offset, size_t len,
                   Finder& finder);
    QPDF_DLL
    bool findLast(char const* start_chars,
                  qpdf_offset_t offset, size_t len,
                  Finder& finder);

    virtual qpdf_offset_t findAndSkipNextEOL() = 0;
    virtual std::string const& getName() const = 0;
    virtual qpdf_offset_t tell() = 0;
    virtual void seek(qpdf_offset_t offset, int whence) = 0;
    virtual void rewind() = 0;
    virtual size_t read(char* buffer, size_t length) = 0;
    virtual void unreadCh(char ch) = 0;

  protected:
    qpdf_offset_t last_offset;
};

#endif // QPDF_INPUTSOURCE_HH

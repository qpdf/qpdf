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

#ifndef QPDF_CLOSEDFILEINPUTSOURCE_HH
#define QPDF_CLOSEDFILEINPUTSOURCE_HH

// This is an input source that reads from files, like FileInputSource, except that it opens and
// closes the file surrounding every operation. This decreases efficiency, but it allows many more
// of these to exist at once than the maximum number of open file descriptors. This is used for
// merging large numbers of files.

#include <qpdf/InputSource.hh>

#include <memory>

class FileInputSource;

class QPDF_DLL_CLASS ClosedFileInputSource: public InputSource
{
  public:
    QPDF_DLL
    ClosedFileInputSource(char const* filename);
    QPDF_DLL
    ~ClosedFileInputSource() override;
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

    // The file stays open between calls to stayOpen(true) and stayOpen(false). You can use this to
    // surround multiple operations on a single ClosedFileInputSource to reduce the overhead of a
    // separate open/close on each call.
    QPDF_DLL
    void stayOpen(bool);

  private:
    ClosedFileInputSource(ClosedFileInputSource const&) = delete;
    ClosedFileInputSource& operator=(ClosedFileInputSource const&) = delete;

    QPDF_DLL_PRIVATE
    void before();
    QPDF_DLL_PRIVATE
    void after();

    std::string filename;
    qpdf_offset_t offset;
    std::shared_ptr<FileInputSource> fis;
    bool stay_open;
};

#endif // QPDF_CLOSEDFILEINPUTSOURCE_HH

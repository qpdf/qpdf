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

#ifndef QPDF_FILEINPUTSOURCE_HH
#define QPDF_FILEINPUTSOURCE_HH

#include <qpdf/InputSource.hh>

class QPDF_DLL_CLASS FileInputSource: public InputSource
{
  public:
    FileInputSource() = default;
    QPDF_DLL
    FileInputSource(char const* filename);
    QPDF_DLL
    FileInputSource(char const* description, FILE* filep, bool close_file);
    QPDF_DLL
    void setFilename(char const* filename);
    QPDF_DLL
    void setFile(char const* description, FILE* filep, bool close_file);

    FileInputSource(FileInputSource const&) = delete;
    FileInputSource& operator=(FileInputSource const&) = delete;

    QPDF_DLL
    ~FileInputSource() override;
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
    bool close_file{false};
    std::string filename;
    FILE* file{nullptr};
};

#endif // QPDF_FILEINPUTSOURCE_HH

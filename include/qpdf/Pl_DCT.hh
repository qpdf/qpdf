// Copyright (c) 2005-2024 Jay Berkenbilt
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

#ifndef PL_DCT_HH
#define PL_DCT_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/Pl_Buffer.hh>
#include <cstddef>

// jpeglib.h must be included after cstddef or else it messes up the definition of size_t.
#include <jpeglib.h>

class QPDF_DLL_CLASS Pl_DCT: public Pipeline
{
  public:
    // Constructor for decompressing image data
    QPDF_DLL
    Pl_DCT(char const* identifier, Pipeline* next);

    // Limit the memory used by jpeglib when decompressing data.
    // NB This is a static option affecting all Pl_DCT instances.
    QPDF_DLL
    static void setMemoryLimit(long limit);

    // Limit the number of scans used by jpeglib when decompressing progressive jpegs.
    // NB This is a static option affecting all Pl_DCT instances.
    QPDF_DLL
    static void setScanLimit(int limit);

    // Treat corrupt data as a runtime error rather than attempting to decompress regardless. This
    // is the qpdf default behaviour. To attempt to decompress corrupt data set 'treat_as_error' to
    // false.
    // NB This is a static option affecting all Pl_DCT instances.
    QPDF_DLL
    static void setThrowOnCorruptData(bool treat_as_error);

    // Constructor for compressing image data
    QPDF_DLL
    Pl_DCT(
        char const* identifier,
        Pipeline* next,
        JDIMENSION image_width,
        JDIMENSION image_height,
        int components,
        J_COLOR_SPACE color_space);

    QPDF_DLL
    ~Pl_DCT() override;

    QPDF_DLL
    void write(unsigned char const* data, size_t len) override;
    QPDF_DLL
    void finish() override;

  private:
    QPDF_DLL_PRIVATE
    void compress(void* cinfo, Buffer*);
    QPDF_DLL_PRIVATE
    void decompress(void* cinfo, Buffer*);

    enum action_e { a_compress, a_decompress };

    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_DCT;

      public:
        QPDF_DLL
        ~Members() = default;
        Members(Members const&) = delete;

      private:
        // For compression
        Members(
            JDIMENSION image_width,
            JDIMENSION image_height,
            int components,
            J_COLOR_SPACE color_space);
        // For decompression
        Members();

        action_e action;
        Pl_Buffer buf;

        // Used for compression
        JDIMENSION image_width{0};
        JDIMENSION image_height{0};
        int components{1};
        J_COLOR_SPACE color_space{JCS_GRAYSCALE};
    };

    std::shared_ptr<Members> m;
};

#endif // PL_DCT_HH

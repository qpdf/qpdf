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

#ifndef PL_DCT_HH
#define PL_DCT_HH

#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Buffer.hh>
#include <jpeglib.h>

class Pl_DCT: public Pipeline
{
  public:
    // Constructor for decompressing image data
    QPDF_DLL
    Pl_DCT(char const* identifier, Pipeline* next);

    class CompressConfig
    {
      public:
        CompressConfig()
        {
        }
        virtual ~CompressConfig()
        {
        }
        virtual void apply(jpeg_compress_struct*) = 0;
    };

    // Constructor for compressing image data
    QPDF_DLL
    Pl_DCT(char const* identifier, Pipeline* next,
           JDIMENSION image_width,
           JDIMENSION image_height,
           int components,
           J_COLOR_SPACE color_space,
           CompressConfig* config_callback = 0);

    QPDF_DLL
    virtual ~Pl_DCT();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    void compress(void* cinfo, Buffer*);
    void decompress(void* cinfo, Buffer*);

    enum action_e { a_compress, a_decompress };

    action_e action;
    Pl_Buffer buf;

    // Used for compression
    JDIMENSION image_width;
    JDIMENSION image_height;
    int components;
    J_COLOR_SPACE color_space;

    CompressConfig* config_callback;

};

#endif // PL_DCT_HH

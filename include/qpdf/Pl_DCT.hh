// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __PL_DCT_HH__
#define __PL_DCT_HH__

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
    void compress(void* cinfo, PointerHolder<Buffer>);
    void decompress(void* cinfo, PointerHolder<Buffer>);

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

#endif // __PL_DCT_HH__

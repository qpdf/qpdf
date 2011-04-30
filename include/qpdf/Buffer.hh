// Copyright (c) 2005-2011 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __BUFFER_HH__
#define __BUFFER_HH__

#include <qpdf/DLL.h>

class Buffer
{
  public:
    QPDF_DLL
    Buffer();

    // Create a Buffer object whose memory is owned by the class and
    // will be freed when the Buffer object is destroyed.
    QPDF_DLL
    Buffer(unsigned long size);

    // Create a Buffer object whose memory is owned by the caller and
    // will not be freed when the Buffer is destroyed.
    QPDF_DLL
    Buffer(unsigned char* buf, unsigned long size);

    QPDF_DLL
    Buffer(Buffer const&);
    QPDF_DLL
    Buffer& operator=(Buffer const&);
    QPDF_DLL
    ~Buffer();
    QPDF_DLL
    unsigned long getSize() const;
    QPDF_DLL
    unsigned char const* getBuffer() const;
    QPDF_DLL
    unsigned char* getBuffer();

  private:
    void init(unsigned long size, unsigned char* buf, bool own_memory);
    void copy(Buffer const&);
    void destroy();

    bool own_memory;
    unsigned long size;
    unsigned char* buf;
};

#endif // __BUFFER_HH__

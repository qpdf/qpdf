// Copyright (c) 2005-2009 Jay Berkenbilt
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
    DLL_EXPORT
    Buffer();
    DLL_EXPORT
    Buffer(unsigned long size);
    DLL_EXPORT
    Buffer(Buffer const&);
    DLL_EXPORT
    Buffer& operator=(Buffer const&);
    DLL_EXPORT
    ~Buffer();
    DLL_EXPORT
    unsigned long getSize() const;
    DLL_EXPORT
    unsigned char const* getBuffer() const;
    DLL_EXPORT
    unsigned char* getBuffer();

  private:
    void init(unsigned long size);
    void copy(Buffer const&);
    void destroy();

    unsigned long size;
    unsigned char* buf;
};

#endif // __BUFFER_HH__

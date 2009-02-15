// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __BUFFER_HH__
#define __BUFFER_HH__

class Buffer
{
  public:
    Buffer();
    Buffer(unsigned long size);
    Buffer(Buffer const&);
    Buffer& operator=(Buffer const&);
    ~Buffer();
    unsigned long getSize() const;
    unsigned char const* getBuffer() const;
    unsigned char* getBuffer();

  private:
    void init(unsigned long size);
    void copy(Buffer const&);
    void destroy();

    unsigned long size;
    unsigned char* buf;
};

#endif // __BUFFER_HH__

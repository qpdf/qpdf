// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __PL_BUFFER_HH__
#define __PL_BUFFER_HH__

// This pipeline accumulates the data passed to it into a memory
// buffer.  Each subsequent use of this buffer appends to the data
// accumulated so far.  getBuffer() may be called only after calling
// finish() and before calling any subsequent write().  At that point,
// a dynamically allocated Buffer object is returned and the internal
// buffer is reset.  The caller is responsible for deleting the
// returned Buffer.
//
// For this pipeline, "next" may be null.  If a next pointer is
// provided, this pipeline will also pass the data through to it.

#include <qpdf/Pipeline.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <list>

class DLL_EXPORT Pl_Buffer: public Pipeline
{
  public:
    Pl_Buffer(char const* identifier, Pipeline* next = 0);
    virtual ~Pl_Buffer();
    virtual void write(unsigned char*, int);
    virtual void finish();

    // Each call to getBuffer() resets this object -- see notes above.
    // The caller is responsible for deleting the returned Buffer
    // object.
    Buffer* getBuffer();

  private:
    bool ready;
    std::list<PointerHolder<Buffer> > data;
    size_t total_size;
};

#endif // __PL_BUFFER_HH__

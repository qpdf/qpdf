// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

// Generalized Pipeline interface.  By convention, subclasses of
// Pipeline are called Pl_Something.
//
// When an instance of Pipeline is created with a pointer to a next
// pipeline, that pipeline writes its data to the next one when it
// finishes with it.  In order to make possible a usage style in which
// a pipeline may be passed to a function which may stick other
// pipelines in front of it, the allocator of a pipeline is
// responsible for its destruction.  In other words, one pipeline
// object does not attempt to manage the memory of its successor.
//
// The client is required to call finish() before destroying a
// Pipeline in order to avoid loss of data.  A Pipeline class should
// not throw an exception in the destructor if this hasn't been done
// though since doing so causes too much trouble when deleting
// pipelines during error conditions.
//
// Some pipelines are reusable (i.e., you can call write() after
// calling finish() and can call finish() multiple times) while others
// are not.  It is up to the caller to use a pipeline according to its
// own restrictions.

#ifndef __PIPELINE_HH__
#define __PIPELINE_HH__

#include <qpdf/DLL.h>
#include <string>

class DLL_EXPORT Pipeline
{
  public:
    Pipeline(char const* identifier, Pipeline* next);

    virtual ~Pipeline();

    // Subclasses should implement write and finish to do their jobs
    // and then, if they are not end-of-line pipelines, call
    // getNext()->write or getNext()->finish.
    virtual void write(unsigned char* data, int len) = 0;
    virtual void finish() = 0;

  protected:
    Pipeline* getNext(bool allow_null = false);
    std::string identifier;

  private:
    // Do not implement copy or assign
    Pipeline(Pipeline const&);
    Pipeline& operator=(Pipeline const&);

    Pipeline* next;
};

#endif // __PIPELINE_HH__

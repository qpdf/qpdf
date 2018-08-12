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

#ifndef PIPELINE_HH
#define PIPELINE_HH

#include <qpdf/DLL.h>
#include <string>

class Pipeline
{
  public:
    QPDF_DLL
    Pipeline(char const* identifier, Pipeline* next);

    QPDF_DLL
    virtual ~Pipeline();

    // Subclasses should implement write and finish to do their jobs
    // and then, if they are not end-of-line pipelines, call
    // getNext()->write or getNext()->finish.  It would be really nice
    // if write could take unsigned char const*, but this would make
    // it much more difficult to write pipelines around legacy
    // interfaces whose calls don't want pointers to const data.  As a
    // rule, pipelines should generally not be modifying the data
    // passed to them.  They should, instead, create new data to pass
    // downstream.
    QPDF_DLL
    virtual void write(unsigned char* data, size_t len) = 0;
    QPDF_DLL
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

#endif // PIPELINE_HH

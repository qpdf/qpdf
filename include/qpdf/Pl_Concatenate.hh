/* Copyright (c) 2005-2015 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __PL_CONCATENATE_HH__
#define __PL_CONCATENATE_HH__

// This pipeline will drop all regular finished calls rather than
// passing them onto next.  To finish downstream streams, call
// manualFinish.  This makes it possible to pipe multiple streams
// (e.g. with QPDFObjectHandle::pipeStreamData) to a downstream like
// Pl_Flate that can't handle multiple calls to finish().

#include <qpdf/Pipeline.hh>

class Pl_Concatenate: public Pipeline
{
  public:
    QPDF_DLL
    Pl_Concatenate(char const* identifier, Pipeline* next);
    QPDF_DLL
    virtual ~Pl_Concatenate();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);

    QPDF_DLL
    virtual void finish();

    // At the very end, call manualFinish to actually finish the rest of
    // the pipeline.
    QPDF_DLL
    void manualFinish();
};

#endif // __PL_CONCATENATE_HH__

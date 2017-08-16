// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __PL_RUNLENGTH_HH__
#define __PL_RUNLENGTH_HH__

#include <qpdf/Pipeline.hh>

class Pl_RunLength: public Pipeline
{
  public:
    enum action_e { a_encode, a_decode };

    QPDF_DLL
    Pl_RunLength(char const* identifier, Pipeline* next,
                 action_e action);
    QPDF_DLL
    virtual ~Pl_RunLength();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    void encode(unsigned char* data, size_t len);
    void decode(unsigned char* data, size_t len);
    void flush_encode();

    enum state_e { st_top, st_copying, st_run };

    action_e action;
    state_e state;
    unsigned char buf[128];
    unsigned int length;
};

#endif // __PL_RUNLENGTH_HH__

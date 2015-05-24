// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __PL_FLATE_HH__
#define __PL_FLATE_HH__

#include <qpdf/Pipeline.hh>

class Pl_Flate: public Pipeline
{
  public:
    static int const def_bufsize = 65536;

    enum action_e { a_inflate, a_deflate };

    QPDF_DLL
    Pl_Flate(char const* identifier, Pipeline* next,
	     action_e action, int out_bufsize = def_bufsize);
    QPDF_DLL
    virtual ~Pl_Flate();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    void handleData(unsigned char* data, int len, int flush);
    void checkError(char const* prefix, int error_code);

    unsigned char* outbuf;
    int out_bufsize;
    action_e action;
    bool initialized;
    void* zdata;
};

#endif // __PL_FLATE_HH__

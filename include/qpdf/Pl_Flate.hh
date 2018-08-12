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

#ifndef PL_FLATE_HH
#define PL_FLATE_HH

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

#endif // PL_FLATE_HH

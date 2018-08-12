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

#ifndef QPDFOBJGEN_HH
#define QPDFOBJGEN_HH

#include <qpdf/DLL.h>

// This class represents an object ID and generation pair.  It is
// suitable to use as a key in a map or set.

class QPDFObjGen
{
  public:
    QPDF_DLL
    QPDFObjGen();
    QPDF_DLL
    QPDFObjGen(int obj, int gen);
    QPDF_DLL
    bool operator<(QPDFObjGen const&) const;
    QPDF_DLL
    bool operator==(QPDFObjGen const&) const;
    QPDF_DLL
    int getObj() const;
    QPDF_DLL
    int getGen() const;

  private:
    int obj;
    int gen;
};

#endif // QPDFOBJGEN_HH

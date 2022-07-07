// Copyright (c) 2005-2022 Jay Berkenbilt
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
#include <qpdf/QUtil.hh>
#include <iostream>

// This class represents an object ID and generation pair.  It is
// suitable to use as a key in a map or set.

class QPDFObjGen
{
  public:
    QPDF_DLL
    QPDFObjGen() :
    obj(0),
    gen(0)
    {
    }
    QPDF_DLL
    QPDFObjGen(int obj, int gen) :
    obj(obj),
    gen(gen)
    {
    }
    QPDF_DLL
    bool operator<(QPDFObjGen const& rhs) const
    {
        return ((obj < rhs.obj) || ((obj == rhs.obj) && (gen < rhs.gen)));
    }
    QPDF_DLL
    bool operator==(QPDFObjGen const& rhs) const
    {
        return ((obj == rhs.obj) && (gen == rhs.gen));
    }
    QPDF_DLL
    int getObj() const
    {
        return obj;
    }
    QPDF_DLL
    int getGen() const
    {
        return gen;
    }
    QPDF_DLL
    std::string unparse() const
    {
        return QUtil::int_to_string(obj) + "," + QUtil::int_to_string(gen);
    }
    QPDF_DLL
    friend std::ostream& operator<<(std::ostream& os, const QPDFObjGen& og)
    {
        os << og.obj << "," << og.gen;
        return os;
    }

  private:
    // This class does not use the Members pattern to avoid a memory
    // allocation for every one of these. A lot of these get created
    // and destroyed.
    int obj;
    int gen;
};

#endif // QPDFOBJGEN_HH

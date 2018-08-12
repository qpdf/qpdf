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

#ifndef QPDFXREFENTRY_HH
#define QPDFXREFENTRY_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>

class QPDFXRefEntry
{
  public:
    // Type constants are from the PDF spec section
    // "Cross-Reference Streams":
    // 0 = free entry; not used
    // 1 = "uncompressed"; field 1 = offset
    // 2 = "compressed"; field 1 = object stream number, field 2 = index

    QPDF_DLL
    QPDFXRefEntry();
    QPDF_DLL
    QPDFXRefEntry(int type, qpdf_offset_t field1, int field2);

    QPDF_DLL
    int getType() const;
    QPDF_DLL
    qpdf_offset_t getOffset() const;    // only for type 1
    QPDF_DLL
    int getObjStreamNumber() const;     // only for type 2
    QPDF_DLL
    int getObjStreamIndex() const;	// only for type 2

  private:
    int type;
    qpdf_offset_t field1;
    int field2;
};

#endif // QPDFXREFENTRY_HH

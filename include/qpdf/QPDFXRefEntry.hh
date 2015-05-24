// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFXREFENTRY_HH__
#define __QPDFXREFENTRY_HH__

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

#endif // __QPDFXREFENTRY_HH__

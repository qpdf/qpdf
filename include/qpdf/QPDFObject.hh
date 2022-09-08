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

#ifndef QPDFOBJECT_OLD_HH
#define QPDFOBJECT_OLD_HH

// **********************************************************************
//
//   This file is for backward compatibility. The header file for the
//   internal QPDFObject class (not part of the public API) is in
//   QPDFObject_private.hh (not installed).
//
// **********************************************************************

// ABI: in qpdf 12, leave this file in place and have it generate an
// error. This is to prevent someone from being able to successfully
// include this file and get a copy from a previous installation
// thereby accidentally creating sources depend on having an older
// version installed. When enough time has passed, this file can be
// removed, and libqpdf/qpdf/QPDFObject_private.hh can be renamed to
// libqpdf/qpdf/QPDFObject.hh.

#ifndef QPDF_OBJECT_NOWARN
// ABI: remove this file in qpdf 12
# warning "QPDFObject.hh is deprecated see comments in QPDFObject.hh"
#endif

#include <qpdf/Constants.h>
class QPDFObject
{
  public:
    // This file and these symbols will be removed in QPDF 12. Instead
    // of including this header, include <qpdf/Constants.h>. Replace
    // `QPDFObject::ot_` with `::ot_` in your code.

    // Prior to qpdf 10.5, qpdf_object_type_e was
    // QPDFObject::object_type_e but was moved to make it accessible
    // to the C API. The code below is for backward compatibility
    // until qpdf 12.
    typedef enum qpdf_object_type_e object_type_e;
    static constexpr object_type_e ot_uninitialized = ::ot_uninitialized;
    static constexpr object_type_e ot_reserved = ::ot_reserved;
    static constexpr object_type_e ot_null = ::ot_null;
    static constexpr object_type_e ot_boolean = ::ot_boolean;
    static constexpr object_type_e ot_integer = ::ot_integer;
    static constexpr object_type_e ot_real = ::ot_real;
    static constexpr object_type_e ot_string = ::ot_string;
    static constexpr object_type_e ot_name = ::ot_name;
    static constexpr object_type_e ot_array = ::ot_array;
    static constexpr object_type_e ot_dictionary = ::ot_dictionary;
    static constexpr object_type_e ot_stream = ::ot_stream;
    static constexpr object_type_e ot_operator = ::ot_operator;
    static constexpr object_type_e ot_inlineimage = ::ot_inlineimage;

  private:
    QPDFObject() = delete;
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;
};

#endif // QPDFOBJECT_OLD_HH
